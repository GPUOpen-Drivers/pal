/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
namespace PalAbi
{

namespace Metadata
{

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::PipelineType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("VsPs"):
            *pValue = Abi::PipelineType::VsPs;
            break;
        case HashLiteralString("Gs"):
            *pValue = Abi::PipelineType::Gs;
            break;
        case HashLiteralString("Cs"):
            *pValue = Abi::PipelineType::Cs;
            break;
        case HashLiteralString("Ngg"):
            *pValue = Abi::PipelineType::Ngg;
            break;
        case HashLiteralString("Tess"):
            *pValue = Abi::PipelineType::Tess;
            break;
        case HashLiteralString("GsTess"):
            *pValue = Abi::PipelineType::GsTess;
            break;
        case HashLiteralString("NggTess"):
            *pValue = Abi::PipelineType::NggTess;
            break;
        case HashLiteralString("Mesh"):
            *pValue = Abi::PipelineType::Mesh;
            break;
        case HashLiteralString("TaskMesh"):
            *pValue = Abi::PipelineType::TaskMesh;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::PipelineType  value)
{
    switch (value)
    {
    case Abi::PipelineType::VsPs:
        pWriter->Pack("VsPs");
        break;
    case Abi::PipelineType::Gs:
        pWriter->Pack("Gs");
        break;
    case Abi::PipelineType::Cs:
        pWriter->Pack("Cs");
        break;
    case Abi::PipelineType::Ngg:
        pWriter->Pack("Ngg");
        break;
    case Abi::PipelineType::Tess:
        pWriter->Pack("Tess");
        break;
    case Abi::PipelineType::GsTess:
        pWriter->Pack("GsTess");
        break;
    case Abi::PipelineType::NggTess:
        pWriter->Pack("NggTess");
        break;
    case Abi::PipelineType::Mesh:
        pWriter->Pack("Mesh");
        break;
    case Abi::PipelineType::TaskMesh:
        pWriter->Pack("TaskMesh");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::ApiShaderType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString(".compute"):
            *pValue = Abi::ApiShaderType::Cs;
            break;
        case HashLiteralString(".task"):
            *pValue = Abi::ApiShaderType::Task;
            break;
        case HashLiteralString(".vertex"):
            *pValue = Abi::ApiShaderType::Vs;
            break;
        case HashLiteralString(".hull"):
            *pValue = Abi::ApiShaderType::Hs;
            break;
        case HashLiteralString(".domain"):
            *pValue = Abi::ApiShaderType::Ds;
            break;
        case HashLiteralString(".geometry"):
            *pValue = Abi::ApiShaderType::Gs;
            break;
        case HashLiteralString(".mesh"):
            *pValue = Abi::ApiShaderType::Mesh;
            break;
        case HashLiteralString(".pixel"):
            *pValue = Abi::ApiShaderType::Ps;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::ApiShaderType  value)
{
    switch (value)
    {
    case Abi::ApiShaderType::Cs:
        pWriter->Pack(".compute");
        break;
    case Abi::ApiShaderType::Task:
        pWriter->Pack(".task");
        break;
    case Abi::ApiShaderType::Vs:
        pWriter->Pack(".vertex");
        break;
    case Abi::ApiShaderType::Hs:
        pWriter->Pack(".hull");
        break;
    case Abi::ApiShaderType::Ds:
        pWriter->Pack(".domain");
        break;
    case Abi::ApiShaderType::Gs:
        pWriter->Pack(".geometry");
        break;
    case Abi::ApiShaderType::Mesh:
        pWriter->Pack(".mesh");
        break;
    case Abi::ApiShaderType::Ps:
        pWriter->Pack(".pixel");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::ApiShaderSubType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("Unknown"):
            *pValue = Abi::ApiShaderSubType::Unknown;
            break;
        case HashLiteralString("Traversal"):
            *pValue = Abi::ApiShaderSubType::Traversal;
            break;
        case HashLiteralString("RayGeneration"):
            *pValue = Abi::ApiShaderSubType::RayGeneration;
            break;
        case HashLiteralString("Intersection"):
            *pValue = Abi::ApiShaderSubType::Intersection;
            break;
        case HashLiteralString("AnyHit"):
            *pValue = Abi::ApiShaderSubType::AnyHit;
            break;
        case HashLiteralString("ClosestHit"):
            *pValue = Abi::ApiShaderSubType::ClosestHit;
            break;
        case HashLiteralString("Miss"):
            *pValue = Abi::ApiShaderSubType::Miss;
            break;
        case HashLiteralString("Callable"):
            *pValue = Abi::ApiShaderSubType::Callable;
            break;
        case HashLiteralString("LaunchKernel"):
            *pValue = Abi::ApiShaderSubType::LaunchKernel;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::ApiShaderSubType  value)
{
    switch (value)
    {
    case Abi::ApiShaderSubType::Unknown:
        pWriter->Pack("Unknown");
        break;
    case Abi::ApiShaderSubType::Traversal:
        pWriter->Pack("Traversal");
        break;
    case Abi::ApiShaderSubType::RayGeneration:
        pWriter->Pack("RayGeneration");
        break;
    case Abi::ApiShaderSubType::Intersection:
        pWriter->Pack("Intersection");
        break;
    case Abi::ApiShaderSubType::AnyHit:
        pWriter->Pack("AnyHit");
        break;
    case Abi::ApiShaderSubType::ClosestHit:
        pWriter->Pack("ClosestHit");
        break;
    case Abi::ApiShaderSubType::Miss:
        pWriter->Pack("Miss");
        break;
    case Abi::ApiShaderSubType::Callable:
        pWriter->Pack("Callable");
        break;
    case Abi::ApiShaderSubType::LaunchKernel:
        pWriter->Pack("LaunchKernel");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::HardwareStage*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString(".ls"):
            *pValue = Abi::HardwareStage::Ls;
            break;
        case HashLiteralString(".hs"):
            *pValue = Abi::HardwareStage::Hs;
            break;
        case HashLiteralString(".es"):
            *pValue = Abi::HardwareStage::Es;
            break;
        case HashLiteralString(".gs"):
            *pValue = Abi::HardwareStage::Gs;
            break;
        case HashLiteralString(".vs"):
            *pValue = Abi::HardwareStage::Vs;
            break;
        case HashLiteralString(".ps"):
            *pValue = Abi::HardwareStage::Ps;
            break;
        case HashLiteralString(".cs"):
            *pValue = Abi::HardwareStage::Cs;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::HardwareStage  value)
{
    switch (value)
    {
    case Abi::HardwareStage::Ls:
        pWriter->Pack(".ls");
        break;
    case Abi::HardwareStage::Hs:
        pWriter->Pack(".hs");
        break;
    case Abi::HardwareStage::Es:
        pWriter->Pack(".es");
        break;
    case Abi::HardwareStage::Gs:
        pWriter->Pack(".gs");
        break;
    case Abi::HardwareStage::Vs:
        pWriter->Pack(".vs");
        break;
    case Abi::HardwareStage::Ps:
        pWriter->Pack(".ps");
        break;
    case Abi::HardwareStage::Cs:
        pWriter->Pack(".cs");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::PipelineSymbolType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("unknown"):
            *pValue = Abi::PipelineSymbolType::Unknown;
            break;
        case HashLiteralString("_amdgpu_ls_main"):
            *pValue = Abi::PipelineSymbolType::LsMainEntry;
            break;
        case HashLiteralString("_amdgpu_hs_main"):
            *pValue = Abi::PipelineSymbolType::HsMainEntry;
            break;
        case HashLiteralString("_amdgpu_es_main"):
            *pValue = Abi::PipelineSymbolType::EsMainEntry;
            break;
        case HashLiteralString("_amdgpu_gs_main"):
            *pValue = Abi::PipelineSymbolType::GsMainEntry;
            break;
        case HashLiteralString("_amdgpu_vs_main"):
            *pValue = Abi::PipelineSymbolType::VsMainEntry;
            break;
        case HashLiteralString("_amdgpu_ps_main"):
            *pValue = Abi::PipelineSymbolType::PsMainEntry;
            break;
        case HashLiteralString("_amdgpu_cs_main"):
            *pValue = Abi::PipelineSymbolType::CsMainEntry;
            break;
        case HashLiteralString("_amdgpu_ls_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::LsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_hs_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::HsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_es_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::EsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_gs_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::GsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_vs_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::VsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_ps_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::PsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_cs_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::CsShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_ps_dual_source_shdr_intrl_tbl"):
            *pValue = Abi::PipelineSymbolType::PsDualSourceShdrIntrlTblPtr;
            break;
        case HashLiteralString("_amdgpu_ls_disasm"):
            *pValue = Abi::PipelineSymbolType::LsDisassembly;
            break;
        case HashLiteralString("_amdgpu_hs_disasm"):
            *pValue = Abi::PipelineSymbolType::HsDisassembly;
            break;
        case HashLiteralString("_amdgpu_es_disasm"):
            *pValue = Abi::PipelineSymbolType::EsDisassembly;
            break;
        case HashLiteralString("_amdgpu_gs_disasm"):
            *pValue = Abi::PipelineSymbolType::GsDisassembly;
            break;
        case HashLiteralString("_amdgpu_vs_disasm"):
            *pValue = Abi::PipelineSymbolType::VsDisassembly;
            break;
        case HashLiteralString("_amdgpu_ps_disasm"):
            *pValue = Abi::PipelineSymbolType::PsDisassembly;
            break;
        case HashLiteralString("_amdgpu_cs_disasm"):
            *pValue = Abi::PipelineSymbolType::CsDisassembly;
            break;
        case HashLiteralString("_amdgpu_ps_dual_source_disasm"):
            *pValue = Abi::PipelineSymbolType::PsDualSourceDisassembly;
            break;
        case HashLiteralString("_amdgpu_ls_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::LsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_hs_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::HsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_es_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::EsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_gs_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::GsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_vs_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::VsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_ps_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::PsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_cs_shdr_intrl_data"):
            *pValue = Abi::PipelineSymbolType::CsShdrIntrlData;
            break;
        case HashLiteralString("_amdgpu_pipeline_intrl_data"):
            *pValue = Abi::PipelineSymbolType::PipelineIntrlData;
            break;
        case HashLiteralString("color_export_shader"):
            *pValue = Abi::PipelineSymbolType::PsColorExportEntry;
            break;
        case HashLiteralString("color_export_shader_dual_source"):
            *pValue = Abi::PipelineSymbolType::PsColorExportDualSourceEntry;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::PipelineSymbolType  value)
{
    switch (value)
    {
    case Abi::PipelineSymbolType::Unknown:
        pWriter->Pack("unknown");
        break;
    case Abi::PipelineSymbolType::LsMainEntry:
        pWriter->Pack("_amdgpu_ls_main");
        break;
    case Abi::PipelineSymbolType::HsMainEntry:
        pWriter->Pack("_amdgpu_hs_main");
        break;
    case Abi::PipelineSymbolType::EsMainEntry:
        pWriter->Pack("_amdgpu_es_main");
        break;
    case Abi::PipelineSymbolType::GsMainEntry:
        pWriter->Pack("_amdgpu_gs_main");
        break;
    case Abi::PipelineSymbolType::VsMainEntry:
        pWriter->Pack("_amdgpu_vs_main");
        break;
    case Abi::PipelineSymbolType::PsMainEntry:
        pWriter->Pack("_amdgpu_ps_main");
        break;
    case Abi::PipelineSymbolType::CsMainEntry:
        pWriter->Pack("_amdgpu_cs_main");
        break;
    case Abi::PipelineSymbolType::LsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_ls_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::HsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_hs_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::EsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_es_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::GsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_gs_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::VsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_vs_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::PsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_ps_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::CsShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_cs_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::PsDualSourceShdrIntrlTblPtr:
        pWriter->Pack("_amdgpu_ps_dual_source_shdr_intrl_tbl");
        break;
    case Abi::PipelineSymbolType::LsDisassembly:
        pWriter->Pack("_amdgpu_ls_disasm");
        break;
    case Abi::PipelineSymbolType::HsDisassembly:
        pWriter->Pack("_amdgpu_hs_disasm");
        break;
    case Abi::PipelineSymbolType::EsDisassembly:
        pWriter->Pack("_amdgpu_es_disasm");
        break;
    case Abi::PipelineSymbolType::GsDisassembly:
        pWriter->Pack("_amdgpu_gs_disasm");
        break;
    case Abi::PipelineSymbolType::VsDisassembly:
        pWriter->Pack("_amdgpu_vs_disasm");
        break;
    case Abi::PipelineSymbolType::PsDisassembly:
        pWriter->Pack("_amdgpu_ps_disasm");
        break;
    case Abi::PipelineSymbolType::CsDisassembly:
        pWriter->Pack("_amdgpu_cs_disasm");
        break;
    case Abi::PipelineSymbolType::PsDualSourceDisassembly:
        pWriter->Pack("_amdgpu_ps_dual_source_disasm");
        break;
    case Abi::PipelineSymbolType::LsShdrIntrlData:
        pWriter->Pack("_amdgpu_ls_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::HsShdrIntrlData:
        pWriter->Pack("_amdgpu_hs_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::EsShdrIntrlData:
        pWriter->Pack("_amdgpu_es_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::GsShdrIntrlData:
        pWriter->Pack("_amdgpu_gs_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::VsShdrIntrlData:
        pWriter->Pack("_amdgpu_vs_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::PsShdrIntrlData:
        pWriter->Pack("_amdgpu_ps_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::CsShdrIntrlData:
        pWriter->Pack("_amdgpu_cs_shdr_intrl_data");
        break;
    case Abi::PipelineSymbolType::PipelineIntrlData:
        pWriter->Pack("_amdgpu_pipeline_intrl_data");
        break;
    case Abi::PipelineSymbolType::PsColorExportEntry:
        pWriter->Pack("color_export_shader");
        break;
    case Abi::PipelineSymbolType::PsColorExportDualSourceEntry:
        pWriter->Pack("color_export_shader_dual_source");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::PointSpriteSelect*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("Zero"):
            *pValue = Abi::PointSpriteSelect::Zero;
            break;
        case HashLiteralString("One"):
            *pValue = Abi::PointSpriteSelect::One;
            break;
        case HashLiteralString("S"):
            *pValue = Abi::PointSpriteSelect::S;
            break;
        case HashLiteralString("T"):
            *pValue = Abi::PointSpriteSelect::T;
            break;
        case HashLiteralString("None"):
            *pValue = Abi::PointSpriteSelect::None;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::PointSpriteSelect  value)
{
    switch (value)
    {
    case Abi::PointSpriteSelect::Zero:
        pWriter->Pack("Zero");
        break;
    case Abi::PointSpriteSelect::One:
        pWriter->Pack("One");
        break;
    case Abi::PointSpriteSelect::S:
        pWriter->Pack("S");
        break;
    case Abi::PointSpriteSelect::T:
        pWriter->Pack("T");
        break;
    case Abi::PointSpriteSelect::None:
        pWriter->Pack("None");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::GsOutPrimType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("PointList"):
            *pValue = Abi::GsOutPrimType::PointList;
            break;
        case HashLiteralString("LineStrip"):
            *pValue = Abi::GsOutPrimType::LineStrip;
            break;
        case HashLiteralString("TriStrip"):
            *pValue = Abi::GsOutPrimType::TriStrip;
            break;
        case HashLiteralString("Rect2d"):
            *pValue = Abi::GsOutPrimType::Rect2d;
            break;
        case HashLiteralString("RectList"):
            *pValue = Abi::GsOutPrimType::RectList;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::GsOutPrimType  value)
{
    switch (value)
    {
    case Abi::GsOutPrimType::PointList:
        pWriter->Pack("PointList");
        break;
    case Abi::GsOutPrimType::LineStrip:
        pWriter->Pack("LineStrip");
        break;
    case Abi::GsOutPrimType::TriStrip:
        pWriter->Pack("TriStrip");
        break;
    case Abi::GsOutPrimType::Rect2d:
        pWriter->Pack("Rect2d");
        break;
    case Abi::GsOutPrimType::RectList:
        pWriter->Pack("RectList");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::CoverageToShaderSel*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("InputCoverage"):
            *pValue = Abi::CoverageToShaderSel::InputCoverage;
            break;
        case HashLiteralString("InputInnerCoverage"):
            *pValue = Abi::CoverageToShaderSel::InputInnerCoverage;
            break;
        case HashLiteralString("InputDepthCoverage"):
            *pValue = Abi::CoverageToShaderSel::InputDepthCoverage;
            break;
        case HashLiteralString("Raw"):
            *pValue = Abi::CoverageToShaderSel::Raw;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::CoverageToShaderSel  value)
{
    switch (value)
    {
    case Abi::CoverageToShaderSel::InputCoverage:
        pWriter->Pack("InputCoverage");
        break;
    case Abi::CoverageToShaderSel::InputInnerCoverage:
        pWriter->Pack("InputInnerCoverage");
        break;
    case Abi::CoverageToShaderSel::InputDepthCoverage:
        pWriter->Pack("InputDepthCoverage");
        break;
    case Abi::CoverageToShaderSel::Raw:
        pWriter->Pack("Raw");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
}

// =====================================================================================================================
inline Result DeserializeEnum(
    MsgPackReader*  pReader,
    Abi::CbConstUsageType*  pValue)
{
    StringViewType key;
    Result result = pReader->UnpackNext(&key);

    if (result == Result::Success)
    {
        switch (HashString(key))
        {
        case HashLiteralString("LoopIter"):
            *pValue = Abi::CbConstUsageType::LoopIter;
            break;
        case HashLiteralString("Eq0Float"):
            *pValue = Abi::CbConstUsageType::Eq0Float;
            break;
        case HashLiteralString("Lt0Float"):
            *pValue = Abi::CbConstUsageType::Lt0Float;
            break;
        case HashLiteralString("Gt0Float"):
            *pValue = Abi::CbConstUsageType::Gt0Float;
            break;
        case HashLiteralString("Eq0Int"):
            *pValue = Abi::CbConstUsageType::Eq0Int;
            break;
        case HashLiteralString("Lt0Int"):
            *pValue = Abi::CbConstUsageType::Lt0Int;
            break;
        case HashLiteralString("Gt0Int"):
            *pValue = Abi::CbConstUsageType::Gt0Int;
            break;
        case HashLiteralString("Other"):
            *pValue = Abi::CbConstUsageType::Other;
            break;
        default:
            result = Result::NotFound;
            break;
        }
    }

    return result;
}

// =====================================================================================================================
inline Result SerializeEnum(
    MsgPackWriter*  pWriter,
    Abi::CbConstUsageType  value)
{
    switch (value)
    {
    case Abi::CbConstUsageType::LoopIter:
        pWriter->Pack("LoopIter");
        break;
    case Abi::CbConstUsageType::Eq0Float:
        pWriter->Pack("Eq0Float");
        break;
    case Abi::CbConstUsageType::Lt0Float:
        pWriter->Pack("Lt0Float");
        break;
    case Abi::CbConstUsageType::Gt0Float:
        pWriter->Pack("Gt0Float");
        break;
    case Abi::CbConstUsageType::Eq0Int:
        pWriter->Pack("Eq0Int");
        break;
    case Abi::CbConstUsageType::Lt0Int:
        pWriter->Pack("Lt0Int");
        break;
    case Abi::CbConstUsageType::Gt0Int:
        pWriter->Pack("Gt0Int");
        break;
    case Abi::CbConstUsageType::Other:
        pWriter->Pack("Other");
        break;
    default:
        break;
    }

    return pWriter->GetStatus();
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
inline Result DeserializeShaderMetadata(
    MsgPackReader*  pReader,
    ShaderMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(ShaderMetadataKey::ApiShaderHash):
                PAL_ASSERT(pMetadata->hasEntry.apiShaderHash == 0);
                result = pReader->UnpackNext(&pMetadata->apiShaderHash);
                pMetadata->hasEntry.apiShaderHash = (result == Result::Success);;
                break;

            case HashLiteralString(ShaderMetadataKey::HardwareMapping):
                PAL_ASSERT(pMetadata->hasEntry.hardwareMapping == 0);
                result = DeserializeEnumBitflags<Abi::HardwareStage>(pReader, &pMetadata->hardwareMapping);
                pMetadata->hasEntry.hardwareMapping = (result == Result::Success);;
                break;

            case HashLiteralString(ShaderMetadataKey::ShaderSubtype):
                PAL_ASSERT(pMetadata->hasEntry.shaderSubtype == 0);
                result = DeserializeEnum(pReader, &pMetadata->shaderSubtype);
                pMetadata->hasEntry.shaderSubtype = (result == Result::Success);;
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
inline Result DeserializeShaderMetadata(
    MsgPackReader*  pReader,
    ShaderMetadata (*pMetadata)[static_cast<uint32>(Abi::ApiShaderType::Count)])
{
    Result result = ((pReader->Type() == CWP_ITEM_MAP) && (pReader->Get().as.map.size <= ArrayLen(*pMetadata))) ?
                    Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        Abi::ApiShaderType key = Abi::ApiShaderType::Count;
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
inline Result DeserializeCbConstUsageMetadata(
    MsgPackReader*  pReader,
    CbConstUsageMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_ARRAY) ? Result::Success : Result::ErrorInvalidValue;

    const uint32 arraySize = pReader->Get().as.array.size;

    for (uint32 j = 0; ((result == Result::Success) && (j < arraySize)); j++)
    {
        result = pReader->Next(CWP_ITEM_MAP);

        for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            StringViewType key;
            result = pReader->UnpackNext(&key);

            if (result == Result::Success)
            {
                switch (HashString(key))
                {
                case HashLiteralString(CbConstUsageMetadataKey::BufferId):
                    PAL_ASSERT(pMetadata[j].hasEntry.bufferId == 0);
                    result = pReader->UnpackNext(&pMetadata[j].bufferId);
                    pMetadata[j].hasEntry.bufferId = (result == Result::Success);;
                    break;

                case HashLiteralString(CbConstUsageMetadataKey::BufferIndex):
                    PAL_ASSERT(pMetadata[j].hasEntry.bufferIndex == 0);
                    result = pReader->UnpackNext(&pMetadata[j].bufferIndex);
                    pMetadata[j].hasEntry.bufferIndex = (result == Result::Success);;
                    break;

                case HashLiteralString(CbConstUsageMetadataKey::Elem):
                    PAL_ASSERT(pMetadata[j].hasEntry.elem == 0);
                    result = pReader->UnpackNext(&pMetadata[j].elem);
                    pMetadata[j].hasEntry.elem = (result == Result::Success);;
                    break;

                case HashLiteralString(CbConstUsageMetadataKey::Chan):
                    PAL_ASSERT(pMetadata[j].hasEntry.chan == 0);
                    result = pReader->UnpackNext(&pMetadata[j].chan);
                    pMetadata[j].hasEntry.chan = (result == Result::Success);;
                    break;

                case HashLiteralString(CbConstUsageMetadataKey::Usage):
                    PAL_ASSERT(pMetadata[j].hasEntry.usage == 0);
                    result = DeserializeEnum(pReader, &pMetadata[j].usage);
                    pMetadata[j].hasEntry.usage = (result == Result::Success);;
                    break;
                default:
                    result = pReader->Skip(1);
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
inline Result DeserializeHardwareStageMetadata(
    MsgPackReader*  pReader,
    HardwareStageMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(HardwareStageMetadataKey::EntryPoint):
                PAL_ASSERT(pMetadata->hasEntry.entryPoint == 0);
                result = DeserializeEnum(pReader, &pMetadata->entryPoint);
                pMetadata->hasEntry.entryPoint = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::ScratchMemorySize):
                PAL_ASSERT(pMetadata->hasEntry.scratchMemorySize == 0);
                result = pReader->UnpackNext(&pMetadata->scratchMemorySize);
                pMetadata->hasEntry.scratchMemorySize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::BackendStackSize):
                PAL_ASSERT(pMetadata->hasEntry.backendStackSize == 0);
                result = pReader->UnpackNext(&pMetadata->backendStackSize);
                pMetadata->hasEntry.backendStackSize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::FrontendStackSize):
                PAL_ASSERT(pMetadata->hasEntry.frontendStackSize == 0);
                result = pReader->UnpackNext(&pMetadata->frontendStackSize);
                pMetadata->hasEntry.frontendStackSize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::LdsSize):
                PAL_ASSERT(pMetadata->hasEntry.ldsSize == 0);
                result = pReader->UnpackNext(&pMetadata->ldsSize);
                pMetadata->hasEntry.ldsSize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::PerfDataBufferSize):
                PAL_ASSERT(pMetadata->hasEntry.perfDataBufferSize == 0);
                result = pReader->UnpackNext(&pMetadata->perfDataBufferSize);
                pMetadata->hasEntry.perfDataBufferSize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::VgprCount):
                PAL_ASSERT(pMetadata->hasEntry.vgprCount == 0);
                result = pReader->UnpackNext(&pMetadata->vgprCount);
                pMetadata->hasEntry.vgprCount = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::SgprCount):
                PAL_ASSERT(pMetadata->hasEntry.sgprCount == 0);
                result = pReader->UnpackNext(&pMetadata->sgprCount);
                pMetadata->hasEntry.sgprCount = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::VgprLimit):
                PAL_ASSERT(pMetadata->hasEntry.vgprLimit == 0);
                result = pReader->UnpackNext(&pMetadata->vgprLimit);
                pMetadata->hasEntry.vgprLimit = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::SgprLimit):
                PAL_ASSERT(pMetadata->hasEntry.sgprLimit == 0);
                result = pReader->UnpackNext(&pMetadata->sgprLimit);
                pMetadata->hasEntry.sgprLimit = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::ThreadgroupDimensions):
                PAL_ASSERT(pMetadata->hasEntry.threadgroupDimensions == 0);
                result = pReader->UnpackNext(&pMetadata->threadgroupDimensions);
                pMetadata->hasEntry.threadgroupDimensions = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::OrigThreadgroupDimensions):
                PAL_ASSERT(pMetadata->hasEntry.origThreadgroupDimensions == 0);
                result = pReader->UnpackNext(&pMetadata->origThreadgroupDimensions);
                pMetadata->hasEntry.origThreadgroupDimensions = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::CbConstUsages):
                PAL_ASSERT(pMetadata->hasEntry.cbConstUsage == 0);
                pReader->Next();
                result = DeserializeCbConstUsageMetadata(
                        pReader, &pMetadata->cbConstUsage[0]);
                    pMetadata->hasEntry.cbConstUsage = (result == Result::Success);
                break;

            case HashLiteralString(HardwareStageMetadataKey::NumCbConstUsages):
                PAL_ASSERT(pMetadata->hasEntry.numCbConstUsages == 0);
                result = pReader->UnpackNext(&pMetadata->numCbConstUsages);
                pMetadata->hasEntry.numCbConstUsages = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::WavefrontSize):
                PAL_ASSERT(pMetadata->hasEntry.wavefrontSize == 0);
                result = pReader->UnpackNext(&pMetadata->wavefrontSize);
                pMetadata->hasEntry.wavefrontSize = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::UserDataRegMap):
                PAL_ASSERT(pMetadata->hasEntry.userDataRegMap == 0);
                result = pReader->UnpackNext(&pMetadata->userDataRegMap);
                pMetadata->hasEntry.userDataRegMap = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::ChecksumValue):
                PAL_ASSERT(pMetadata->hasEntry.checksumValue == 0);
                result = pReader->UnpackNext(&pMetadata->checksumValue);
                pMetadata->hasEntry.checksumValue = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::FloatMode):
                PAL_ASSERT(pMetadata->hasEntry.floatMode == 0);
                result = pReader->UnpackNext(&pMetadata->floatMode);
                pMetadata->hasEntry.floatMode = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::Fp16Overflow):
            {
                PAL_ASSERT(pMetadata->hasEntry.fp16Overflow == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.fp16Overflow = value;
                }

                pMetadata->hasEntry.fp16Overflow = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::IeeeMode):
            {
                PAL_ASSERT(pMetadata->hasEntry.ieeeMode == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.ieeeMode = value;
                }

                pMetadata->hasEntry.ieeeMode = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::WgpMode):
            {
                PAL_ASSERT(pMetadata->hasEntry.wgpMode == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.wgpMode = value;
                }

                pMetadata->hasEntry.wgpMode = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::MemOrdered):
            {
                PAL_ASSERT(pMetadata->hasEntry.memOrdered == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.memOrdered = value;
                }

                pMetadata->hasEntry.memOrdered = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::ForwardProgress):
            {
                PAL_ASSERT(pMetadata->hasEntry.forwardProgress == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.forwardProgress = value;
                }

                pMetadata->hasEntry.forwardProgress = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::DebugMode):
            {
                PAL_ASSERT(pMetadata->hasEntry.debugMode == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.debugMode = value;
                }

                pMetadata->hasEntry.debugMode = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::ScratchEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.scratchEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.scratchEn = value;
                }

                pMetadata->hasEntry.scratchEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::TrapPresent):
            {
                PAL_ASSERT(pMetadata->hasEntry.trapPresent == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.trapPresent = value;
                }

                pMetadata->hasEntry.trapPresent = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::UserSgprs):
                PAL_ASSERT(pMetadata->hasEntry.userSgprs == 0);
                result = pReader->UnpackNext(&pMetadata->userSgprs);
                pMetadata->hasEntry.userSgprs = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::ExcpEn):
                PAL_ASSERT(pMetadata->hasEntry.excpEn == 0);
                result = pReader->UnpackNext(&pMetadata->excpEn);
                pMetadata->hasEntry.excpEn = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::OffchipLdsEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.offchipLdsEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.offchipLdsEn = value;
                }

                pMetadata->hasEntry.offchipLdsEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(HardwareStageMetadataKey::SharedVgprCnt):
                PAL_ASSERT(pMetadata->hasEntry.sharedVgprCnt == 0);
                result = pReader->UnpackNext(&pMetadata->sharedVgprCnt);
                pMetadata->hasEntry.sharedVgprCnt = (result == Result::Success);;
                break;

            case HashLiteralString(HardwareStageMetadataKey::WavesPerSe):
                PAL_ASSERT(pMetadata->hasEntry.wavesPerSe == 0);
                result = pReader->UnpackNext(&pMetadata->wavesPerSe);
                pMetadata->hasEntry.wavesPerSe = (result == Result::Success);;
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
inline Result DeserializeHardwareStageMetadata(
    MsgPackReader*  pReader,
    HardwareStageMetadata (*pMetadata)[static_cast<uint32>(Abi::HardwareStage::Count)])
{
    Result result = ((pReader->Type() == CWP_ITEM_MAP) && (pReader->Get().as.map.size <= ArrayLen(*pMetadata))) ?
                    Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        Abi::HardwareStage key = Abi::HardwareStage::Count;
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
inline Result DeserializePsInputSemanticMetadata(
    MsgPackReader*  pReader,
    PsInputSemanticMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_ARRAY) ? Result::Success : Result::ErrorInvalidValue;

    const uint32 arraySize = pReader->Get().as.array.size;

    for (uint32 j = 0; ((result == Result::Success) && (j < arraySize)); j++)
    {
        result = pReader->Next(CWP_ITEM_MAP);

        for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            StringViewType key;
            result = pReader->UnpackNext(&key);

            if (result == Result::Success)
            {
                switch (HashString(key))
                {
                case HashLiteralString(PsInputSemanticMetadataKey::Semantic):
                    PAL_ASSERT(pMetadata[j].hasEntry.semantic == 0);
                    result = pReader->UnpackNext(&pMetadata[j].semantic);
                    pMetadata[j].hasEntry.semantic = (result == Result::Success);;
                    break;
                default:
                    result = pReader->Skip(1);
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
inline Result DeserializePrerasterOutputSemanticMetadata(
    MsgPackReader*  pReader,
    PrerasterOutputSemanticMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_ARRAY) ? Result::Success : Result::ErrorInvalidValue;

    const uint32 arraySize = pReader->Get().as.array.size;

    for (uint32 j = 0; ((result == Result::Success) && (j < arraySize)); j++)
    {
        result = pReader->Next(CWP_ITEM_MAP);

        for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            StringViewType key;
            result = pReader->UnpackNext(&key);

            if (result == Result::Success)
            {
                switch (HashString(key))
                {
                case HashLiteralString(PrerasterOutputSemanticMetadataKey::Semantic):
                    PAL_ASSERT(pMetadata[j].hasEntry.semantic == 0);
                    result = pReader->UnpackNext(&pMetadata[j].semantic);
                    pMetadata[j].hasEntry.semantic = (result == Result::Success);;
                    break;

                case HashLiteralString(PrerasterOutputSemanticMetadataKey::Index):
                    PAL_ASSERT(pMetadata[j].hasEntry.index == 0);
                    result = pReader->UnpackNext(&pMetadata[j].index);
                    pMetadata[j].hasEntry.index = (result == Result::Success);;
                    break;
                default:
                    result = pReader->Skip(1);
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
inline Result DeserializePaClClipCntlMetadata(
    MsgPackReader*  pReader,
    PaClClipCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane0Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane0Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane0Ena = value;
                }

                pMetadata->hasEntry.userClipPlane0Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane1Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane1Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane1Ena = value;
                }

                pMetadata->hasEntry.userClipPlane1Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane2Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane2Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane2Ena = value;
                }

                pMetadata->hasEntry.userClipPlane2Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane3Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane3Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane3Ena = value;
                }

                pMetadata->hasEntry.userClipPlane3Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane4Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane4Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane4Ena = value;
                }

                pMetadata->hasEntry.userClipPlane4Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::UserClipPlane5Ena):
            {
                PAL_ASSERT(pMetadata->hasEntry.userClipPlane5Ena == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.userClipPlane5Ena = value;
                }

                pMetadata->hasEntry.userClipPlane5Ena = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::DxLinearAttrClipEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.dxLinearAttrClipEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.dxLinearAttrClipEna = value;
                }

                pMetadata->hasEntry.dxLinearAttrClipEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::ZclipNearDisable):
            {
                PAL_ASSERT(pMetadata->hasEntry.zclipNearDisable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.zclipNearDisable = value;
                }

                pMetadata->hasEntry.zclipNearDisable = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::ZclipFarDisable):
            {
                PAL_ASSERT(pMetadata->hasEntry.zclipFarDisable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.zclipFarDisable = value;
                }

                pMetadata->hasEntry.zclipFarDisable = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::RasterizationKill):
            {
                PAL_ASSERT(pMetadata->hasEntry.rasterizationKill == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.rasterizationKill = value;
                }

                pMetadata->hasEntry.rasterizationKill = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClClipCntlMetadataKey::ClipDisable):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDisable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDisable = value;
                }

                pMetadata->hasEntry.clipDisable = (result == Result::Success);
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
inline Result DeserializePaClVteCntlMetadata(
    MsgPackReader*  pReader,
    PaClVteCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PaClVteCntlMetadataKey::VtxXyFmt):
            {
                PAL_ASSERT(pMetadata->hasEntry.vtxXyFmt == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vtxXyFmt = value;
                }

                pMetadata->hasEntry.vtxXyFmt = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::VtxZFmt):
            {
                PAL_ASSERT(pMetadata->hasEntry.vtxZFmt == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vtxZFmt = value;
                }

                pMetadata->hasEntry.vtxZFmt = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::XScaleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.xScaleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.xScaleEna = value;
                }

                pMetadata->hasEntry.xScaleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::XOffsetEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.xOffsetEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.xOffsetEna = value;
                }

                pMetadata->hasEntry.xOffsetEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::YScaleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.yScaleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.yScaleEna = value;
                }

                pMetadata->hasEntry.yScaleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::YOffsetEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.yOffsetEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.yOffsetEna = value;
                }

                pMetadata->hasEntry.yOffsetEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::ZScaleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.zScaleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.zScaleEna = value;
                }

                pMetadata->hasEntry.zScaleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::ZOffsetEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.zOffsetEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.zOffsetEna = value;
                }

                pMetadata->hasEntry.zOffsetEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVteCntlMetadataKey::VtxW0Fmt):
            {
                PAL_ASSERT(pMetadata->hasEntry.vtxW0Fmt == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vtxW0Fmt = value;
                }

                pMetadata->hasEntry.vtxW0Fmt = (result == Result::Success);
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
inline Result DeserializePaSuVtxCntlMetadata(
    MsgPackReader*  pReader,
    PaSuVtxCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PaSuVtxCntlMetadataKey::PixCenter):
            {
                PAL_ASSERT(pMetadata->hasEntry.pixCenter == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.pixCenter = value;
                }

                pMetadata->hasEntry.pixCenter = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaSuVtxCntlMetadataKey::RoundMode):
                PAL_ASSERT(pMetadata->hasEntry.roundMode == 0);
                result = pReader->UnpackNext(&pMetadata->roundMode);
                pMetadata->hasEntry.roundMode = (result == Result::Success);;
                break;

            case HashLiteralString(PaSuVtxCntlMetadataKey::QuantMode):
                PAL_ASSERT(pMetadata->hasEntry.quantMode == 0);
                result = pReader->UnpackNext(&pMetadata->quantMode);
                pMetadata->hasEntry.quantMode = (result == Result::Success);;
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
inline Result DeserializeVgtShaderStagesEnMetadata(
    MsgPackReader*  pReader,
    VgtShaderStagesEnMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtShaderStagesEnMetadataKey::LsStageEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.lsStageEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.lsStageEn = value;
                }

                pMetadata->hasEntry.lsStageEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::HsStageEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.hsStageEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.hsStageEn = value;
                }

                pMetadata->hasEntry.hsStageEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::EsStageEn):
                PAL_ASSERT(pMetadata->hasEntry.esStageEn == 0);
                result = pReader->UnpackNext(&pMetadata->esStageEn);
                pMetadata->hasEntry.esStageEn = (result == Result::Success);;
                break;

            case HashLiteralString(VgtShaderStagesEnMetadataKey::GsStageEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.gsStageEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.gsStageEn = value;
                }

                pMetadata->hasEntry.gsStageEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::VsStageEn):
                PAL_ASSERT(pMetadata->hasEntry.vsStageEn == 0);
                result = pReader->UnpackNext(&pMetadata->vsStageEn);
                pMetadata->hasEntry.vsStageEn = (result == Result::Success);;
                break;

            case HashLiteralString(VgtShaderStagesEnMetadataKey::DynamicHs):
            {
                PAL_ASSERT(pMetadata->hasEntry.dynamicHs == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.dynamicHs = value;
                }

                pMetadata->hasEntry.dynamicHs = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::MaxPrimgroupInWave):
                PAL_ASSERT(pMetadata->hasEntry.maxPrimgroupInWave == 0);
                result = pReader->UnpackNext(&pMetadata->maxPrimgroupInWave);
                pMetadata->hasEntry.maxPrimgroupInWave = (result == Result::Success);;
                break;

            case HashLiteralString(VgtShaderStagesEnMetadataKey::PrimgenEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.primgenEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.primgenEn = value;
                }

                pMetadata->hasEntry.primgenEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::OrderedIdMode):
            {
                PAL_ASSERT(pMetadata->hasEntry.orderedIdMode == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.orderedIdMode = value;
                }

                pMetadata->hasEntry.orderedIdMode = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::NggWaveIdEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.nggWaveIdEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.nggWaveIdEn = value;
                }

                pMetadata->hasEntry.nggWaveIdEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::GsFastLaunch):
                PAL_ASSERT(pMetadata->hasEntry.gsFastLaunch == 0);
                result = pReader->UnpackNext(&pMetadata->gsFastLaunch);
                pMetadata->hasEntry.gsFastLaunch = (result == Result::Success);;
                break;

            case HashLiteralString(VgtShaderStagesEnMetadataKey::PrimgenPassthruEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.primgenPassthruEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.primgenPassthruEn = value;
                }

                pMetadata->hasEntry.primgenPassthruEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtShaderStagesEnMetadataKey::PrimgenPassthruNoMsg):
            {
                PAL_ASSERT(pMetadata->hasEntry.primgenPassthruNoMsg == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.primgenPassthruNoMsg = value;
                }

                pMetadata->hasEntry.primgenPassthruNoMsg = (result == Result::Success);
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
inline Result DeserializeVgtGsModeMetadata(
    MsgPackReader*  pReader,
    VgtGsModeMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtGsModeMetadataKey::Mode):
                PAL_ASSERT(pMetadata->hasEntry.mode == 0);
                result = pReader->UnpackNext(&pMetadata->mode);
                pMetadata->hasEntry.mode = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsModeMetadataKey::Onchip):
                PAL_ASSERT(pMetadata->hasEntry.onchip == 0);
                result = pReader->UnpackNext(&pMetadata->onchip);
                pMetadata->hasEntry.onchip = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsModeMetadataKey::EsWriteOptimize):
            {
                PAL_ASSERT(pMetadata->hasEntry.esWriteOptimize == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.esWriteOptimize = value;
                }

                pMetadata->hasEntry.esWriteOptimize = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtGsModeMetadataKey::GsWriteOptimize):
            {
                PAL_ASSERT(pMetadata->hasEntry.gsWriteOptimize == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.gsWriteOptimize = value;
                }

                pMetadata->hasEntry.gsWriteOptimize = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtGsModeMetadataKey::CutMode):
                PAL_ASSERT(pMetadata->hasEntry.cutMode == 0);
                result = pReader->UnpackNext(&pMetadata->cutMode);
                pMetadata->hasEntry.cutMode = (result == Result::Success);;
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
inline Result DeserializeVgtTfParamMetadata(
    MsgPackReader*  pReader,
    VgtTfParamMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtTfParamMetadataKey::Type):
                PAL_ASSERT(pMetadata->hasEntry.type == 0);
                result = pReader->UnpackNext(&pMetadata->type);
                pMetadata->hasEntry.type = (result == Result::Success);;
                break;

            case HashLiteralString(VgtTfParamMetadataKey::Partitioning):
                PAL_ASSERT(pMetadata->hasEntry.partitioning == 0);
                result = pReader->UnpackNext(&pMetadata->partitioning);
                pMetadata->hasEntry.partitioning = (result == Result::Success);;
                break;

            case HashLiteralString(VgtTfParamMetadataKey::Topology):
                PAL_ASSERT(pMetadata->hasEntry.topology == 0);
                result = pReader->UnpackNext(&pMetadata->topology);
                pMetadata->hasEntry.topology = (result == Result::Success);;
                break;

            case HashLiteralString(VgtTfParamMetadataKey::DisableDonuts):
            {
                PAL_ASSERT(pMetadata->hasEntry.disableDonuts == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.disableDonuts = value;
                }

                pMetadata->hasEntry.disableDonuts = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtTfParamMetadataKey::NumDsWavesPerSimd):
                PAL_ASSERT(pMetadata->hasEntry.numDsWavesPerSimd == 0);
                result = pReader->UnpackNext(&pMetadata->numDsWavesPerSimd);
                pMetadata->hasEntry.numDsWavesPerSimd = (result == Result::Success);;
                break;

            case HashLiteralString(VgtTfParamMetadataKey::DistributionMode):
                PAL_ASSERT(pMetadata->hasEntry.distributionMode == 0);
                result = pReader->UnpackNext(&pMetadata->distributionMode);
                pMetadata->hasEntry.distributionMode = (result == Result::Success);;
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
inline Result DeserializeVgtLsHsConfigMetadata(
    MsgPackReader*  pReader,
    VgtLsHsConfigMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtLsHsConfigMetadataKey::NumPatches):
                PAL_ASSERT(pMetadata->hasEntry.numPatches == 0);
                result = pReader->UnpackNext(&pMetadata->numPatches);
                pMetadata->hasEntry.numPatches = (result == Result::Success);;
                break;

            case HashLiteralString(VgtLsHsConfigMetadataKey::HsNumInputCp):
                PAL_ASSERT(pMetadata->hasEntry.hsNumInputCp == 0);
                result = pReader->UnpackNext(&pMetadata->hsNumInputCp);
                pMetadata->hasEntry.hsNumInputCp = (result == Result::Success);;
                break;

            case HashLiteralString(VgtLsHsConfigMetadataKey::HsNumOutputCp):
                PAL_ASSERT(pMetadata->hasEntry.hsNumOutputCp == 0);
                result = pReader->UnpackNext(&pMetadata->hsNumOutputCp);
                pMetadata->hasEntry.hsNumOutputCp = (result == Result::Success);;
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
inline Result DeserializeIaMultiVgtParamMetadata(
    MsgPackReader*  pReader,
    IaMultiVgtParamMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(IaMultiVgtParamMetadataKey::PrimgroupSize):
                PAL_ASSERT(pMetadata->hasEntry.primgroupSize == 0);
                result = pReader->UnpackNext(&pMetadata->primgroupSize);
                pMetadata->hasEntry.primgroupSize = (result == Result::Success);;
                break;

            case HashLiteralString(IaMultiVgtParamMetadataKey::PartialVsWaveOn):
            {
                PAL_ASSERT(pMetadata->hasEntry.partialVsWaveOn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.partialVsWaveOn = value;
                }

                pMetadata->hasEntry.partialVsWaveOn = (result == Result::Success);
                break;
            }

            case HashLiteralString(IaMultiVgtParamMetadataKey::PartialEsWaveOn):
            {
                PAL_ASSERT(pMetadata->hasEntry.partialEsWaveOn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.partialEsWaveOn = value;
                }

                pMetadata->hasEntry.partialEsWaveOn = (result == Result::Success);
                break;
            }

            case HashLiteralString(IaMultiVgtParamMetadataKey::SwitchOnEop):
            {
                PAL_ASSERT(pMetadata->hasEntry.switchOnEop == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.switchOnEop = value;
                }

                pMetadata->hasEntry.switchOnEop = (result == Result::Success);
                break;
            }

            case HashLiteralString(IaMultiVgtParamMetadataKey::SwitchOnEoi):
            {
                PAL_ASSERT(pMetadata->hasEntry.switchOnEoi == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.switchOnEoi = value;
                }

                pMetadata->hasEntry.switchOnEoi = (result == Result::Success);
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
inline Result DeserializeSpiInterpControlMetadata(
    MsgPackReader*  pReader,
    SpiInterpControlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiInterpControlMetadataKey::PointSpriteEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.pointSpriteEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.pointSpriteEna = value;
                }

                pMetadata->hasEntry.pointSpriteEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiInterpControlMetadataKey::PointSpriteOverrideX):
                PAL_ASSERT(pMetadata->hasEntry.pointSpriteOverrideX == 0);
                result = DeserializeEnum(pReader, &pMetadata->pointSpriteOverrideX);
                pMetadata->hasEntry.pointSpriteOverrideX = (result == Result::Success);;
                break;

            case HashLiteralString(SpiInterpControlMetadataKey::PointSpriteOverrideY):
                PAL_ASSERT(pMetadata->hasEntry.pointSpriteOverrideY == 0);
                result = DeserializeEnum(pReader, &pMetadata->pointSpriteOverrideY);
                pMetadata->hasEntry.pointSpriteOverrideY = (result == Result::Success);;
                break;

            case HashLiteralString(SpiInterpControlMetadataKey::PointSpriteOverrideZ):
                PAL_ASSERT(pMetadata->hasEntry.pointSpriteOverrideZ == 0);
                result = DeserializeEnum(pReader, &pMetadata->pointSpriteOverrideZ);
                pMetadata->hasEntry.pointSpriteOverrideZ = (result == Result::Success);;
                break;

            case HashLiteralString(SpiInterpControlMetadataKey::PointSpriteOverrideW):
                PAL_ASSERT(pMetadata->hasEntry.pointSpriteOverrideW == 0);
                result = DeserializeEnum(pReader, &pMetadata->pointSpriteOverrideW);
                pMetadata->hasEntry.pointSpriteOverrideW = (result == Result::Success);;
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
inline Result DeserializeSpiPsInputCntlMetadata(
    MsgPackReader*  pReader,
    SpiPsInputCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_ARRAY) ? Result::Success : Result::ErrorInvalidValue;

    const uint32 arraySize = pReader->Get().as.array.size;

    for (uint32 j = 0; ((result == Result::Success) && (j < arraySize)); j++)
    {
        result = pReader->Next(CWP_ITEM_MAP);

        for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
        {
            StringViewType key;
            result = pReader->UnpackNext(&key);

            if (result == Result::Success)
            {
                switch (HashString(key))
                {
                case HashLiteralString(SpiPsInputCntlMetadataKey::Offset):
                    PAL_ASSERT(pMetadata[j].hasEntry.offset == 0);
                    result = pReader->UnpackNext(&pMetadata[j].offset);
                    pMetadata[j].hasEntry.offset = (result == Result::Success);;
                    break;

                case HashLiteralString(SpiPsInputCntlMetadataKey::DefaultVal):
                    PAL_ASSERT(pMetadata[j].hasEntry.defaultVal == 0);
                    result = pReader->UnpackNext(&pMetadata[j].defaultVal);
                    pMetadata[j].hasEntry.defaultVal = (result == Result::Success);;
                    break;

                case HashLiteralString(SpiPsInputCntlMetadataKey::FlatShade):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.flatShade == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.flatShade = value;
                    }

                    pMetadata[j].hasEntry.flatShade = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::CylWrap):
                    PAL_ASSERT(pMetadata[j].hasEntry.cylWrap == 0);
                    result = pReader->UnpackNext(&pMetadata[j].cylWrap);
                    pMetadata[j].hasEntry.cylWrap = (result == Result::Success);;
                    break;

                case HashLiteralString(SpiPsInputCntlMetadataKey::PtSpriteTex):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.ptSpriteTex == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.ptSpriteTex = value;
                    }

                    pMetadata[j].hasEntry.ptSpriteTex = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::Fp16InterpMode):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.fp16InterpMode == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.fp16InterpMode = value;
                    }

                    pMetadata[j].hasEntry.fp16InterpMode = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::Attr0Valid):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.attr0Valid == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.attr0Valid = value;
                    }

                    pMetadata[j].hasEntry.attr0Valid = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::Attr1Valid):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.attr1Valid == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.attr1Valid = value;
                    }

                    pMetadata[j].hasEntry.attr1Valid = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::RotatePcPtr):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.rotatePcPtr == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.rotatePcPtr = value;
                    }

                    pMetadata[j].hasEntry.rotatePcPtr = (result == Result::Success);
                    break;
                }

                case HashLiteralString(SpiPsInputCntlMetadataKey::PrimAttr):
                {
                    PAL_ASSERT(pMetadata[j].hasEntry.primAttr == 0);
                    bool value = false;
                    result = pReader->UnpackNext(&value);

                    if (result == Result::Success)
                    {
                        pMetadata[j].flags.primAttr = value;
                    }

                    pMetadata[j].hasEntry.primAttr = (result == Result::Success);
                    break;
                }
                default:
                    result = pReader->Skip(1);
                    break;
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
inline Result DeserializeSpiShaderGsMeshletDimMetadata(
    MsgPackReader*  pReader,
    SpiShaderGsMeshletDimMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiShaderGsMeshletDimMetadataKey::NumThreadX):
                PAL_ASSERT(pMetadata->hasEntry.numThreadX == 0);
                result = pReader->UnpackNext(&pMetadata->numThreadX);
                pMetadata->hasEntry.numThreadX = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderGsMeshletDimMetadataKey::NumThreadY):
                PAL_ASSERT(pMetadata->hasEntry.numThreadY == 0);
                result = pReader->UnpackNext(&pMetadata->numThreadY);
                pMetadata->hasEntry.numThreadY = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderGsMeshletDimMetadataKey::NumThreadZ):
                PAL_ASSERT(pMetadata->hasEntry.numThreadZ == 0);
                result = pReader->UnpackNext(&pMetadata->numThreadZ);
                pMetadata->hasEntry.numThreadZ = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderGsMeshletDimMetadataKey::ThreadgroupSize):
                PAL_ASSERT(pMetadata->hasEntry.threadgroupSize == 0);
                result = pReader->UnpackNext(&pMetadata->threadgroupSize);
                pMetadata->hasEntry.threadgroupSize = (result == Result::Success);;
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
inline Result DeserializeSpiShaderGsMeshletExpAllocMetadata(
    MsgPackReader*  pReader,
    SpiShaderGsMeshletExpAllocMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiShaderGsMeshletExpAllocMetadataKey::MaxExpVerts):
                PAL_ASSERT(pMetadata->hasEntry.maxExpVerts == 0);
                result = pReader->UnpackNext(&pMetadata->maxExpVerts);
                pMetadata->hasEntry.maxExpVerts = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderGsMeshletExpAllocMetadataKey::MaxExpPrims):
                PAL_ASSERT(pMetadata->hasEntry.maxExpPrims == 0);
                result = pReader->UnpackNext(&pMetadata->maxExpPrims);
                pMetadata->hasEntry.maxExpPrims = (result == Result::Success);;
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
inline Result DeserializeVgtGsInstanceCntMetadata(
    MsgPackReader*  pReader,
    VgtGsInstanceCntMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtGsInstanceCntMetadataKey::Enable):
            {
                PAL_ASSERT(pMetadata->hasEntry.enable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.enable = value;
                }

                pMetadata->hasEntry.enable = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtGsInstanceCntMetadataKey::Count):
                PAL_ASSERT(pMetadata->hasEntry.count == 0);
                result = pReader->UnpackNext(&pMetadata->count);
                pMetadata->hasEntry.count = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsInstanceCntMetadataKey::EnMaxVertOutPerGsInstance):
            {
                PAL_ASSERT(pMetadata->hasEntry.enMaxVertOutPerGsInstance == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.enMaxVertOutPerGsInstance = value;
                }

                pMetadata->hasEntry.enMaxVertOutPerGsInstance = (result == Result::Success);
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
inline Result DeserializeVgtGsOutPrimTypeMetadata(
    MsgPackReader*  pReader,
    VgtGsOutPrimTypeMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtGsOutPrimTypeMetadataKey::OutprimType):
                PAL_ASSERT(pMetadata->hasEntry.outprimType == 0);
                result = DeserializeEnum(pReader, &pMetadata->outprimType);
                pMetadata->hasEntry.outprimType = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOutPrimTypeMetadataKey::OutprimType_1):
                PAL_ASSERT(pMetadata->hasEntry.outprimType_1 == 0);
                result = DeserializeEnum(pReader, &pMetadata->outprimType_1);
                pMetadata->hasEntry.outprimType_1 = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOutPrimTypeMetadataKey::OutprimType_2):
                PAL_ASSERT(pMetadata->hasEntry.outprimType_2 == 0);
                result = DeserializeEnum(pReader, &pMetadata->outprimType_2);
                pMetadata->hasEntry.outprimType_2 = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOutPrimTypeMetadataKey::OutprimType_3):
                PAL_ASSERT(pMetadata->hasEntry.outprimType_3 == 0);
                result = DeserializeEnum(pReader, &pMetadata->outprimType_3);
                pMetadata->hasEntry.outprimType_3 = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOutPrimTypeMetadataKey::UniqueTypePerStream):
            {
                PAL_ASSERT(pMetadata->hasEntry.uniqueTypePerStream == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.uniqueTypePerStream = value;
                }

                pMetadata->hasEntry.uniqueTypePerStream = (result == Result::Success);
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
inline Result DeserializeGeNggSubgrpCntlMetadata(
    MsgPackReader*  pReader,
    GeNggSubgrpCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(GeNggSubgrpCntlMetadataKey::PrimAmpFactor):
                PAL_ASSERT(pMetadata->hasEntry.primAmpFactor == 0);
                result = pReader->UnpackNext(&pMetadata->primAmpFactor);
                pMetadata->hasEntry.primAmpFactor = (result == Result::Success);;
                break;

            case HashLiteralString(GeNggSubgrpCntlMetadataKey::ThreadsPerSubgroup):
                PAL_ASSERT(pMetadata->hasEntry.threadsPerSubgroup == 0);
                result = pReader->UnpackNext(&pMetadata->threadsPerSubgroup);
                pMetadata->hasEntry.threadsPerSubgroup = (result == Result::Success);;
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
inline Result DeserializeVgtGsOnchipCntlMetadata(
    MsgPackReader*  pReader,
    VgtGsOnchipCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtGsOnchipCntlMetadataKey::EsVertsPerSubgroup):
                PAL_ASSERT(pMetadata->hasEntry.esVertsPerSubgroup == 0);
                result = pReader->UnpackNext(&pMetadata->esVertsPerSubgroup);
                pMetadata->hasEntry.esVertsPerSubgroup = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOnchipCntlMetadataKey::GsPrimsPerSubgroup):
                PAL_ASSERT(pMetadata->hasEntry.gsPrimsPerSubgroup == 0);
                result = pReader->UnpackNext(&pMetadata->gsPrimsPerSubgroup);
                pMetadata->hasEntry.gsPrimsPerSubgroup = (result == Result::Success);;
                break;

            case HashLiteralString(VgtGsOnchipCntlMetadataKey::GsInstPrimsPerSubgrp):
                PAL_ASSERT(pMetadata->hasEntry.gsInstPrimsPerSubgrp == 0);
                result = pReader->UnpackNext(&pMetadata->gsInstPrimsPerSubgrp);
                pMetadata->hasEntry.gsInstPrimsPerSubgrp = (result == Result::Success);;
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
inline Result DeserializePaClVsOutCntlMetadata(
    MsgPackReader*  pReader,
    PaClVsOutCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_0):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_0 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_0 = value;
                }

                pMetadata->hasEntry.clipDistEna_0 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_1):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_1 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_1 = value;
                }

                pMetadata->hasEntry.clipDistEna_1 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_2):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_2 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_2 = value;
                }

                pMetadata->hasEntry.clipDistEna_2 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_3):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_3 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_3 = value;
                }

                pMetadata->hasEntry.clipDistEna_3 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_4):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_4 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_4 = value;
                }

                pMetadata->hasEntry.clipDistEna_4 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_5):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_5 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_5 = value;
                }

                pMetadata->hasEntry.clipDistEna_5 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_6):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_6 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_6 = value;
                }

                pMetadata->hasEntry.clipDistEna_6 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::ClipDistEna_7):
            {
                PAL_ASSERT(pMetadata->hasEntry.clipDistEna_7 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.clipDistEna_7 = value;
                }

                pMetadata->hasEntry.clipDistEna_7 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_0):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_0 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_0 = value;
                }

                pMetadata->hasEntry.cullDistEna_0 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_1):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_1 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_1 = value;
                }

                pMetadata->hasEntry.cullDistEna_1 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_2):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_2 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_2 = value;
                }

                pMetadata->hasEntry.cullDistEna_2 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_3):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_3 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_3 = value;
                }

                pMetadata->hasEntry.cullDistEna_3 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_4):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_4 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_4 = value;
                }

                pMetadata->hasEntry.cullDistEna_4 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_5):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_5 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_5 = value;
                }

                pMetadata->hasEntry.cullDistEna_5 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_6):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_6 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_6 = value;
                }

                pMetadata->hasEntry.cullDistEna_6 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::CullDistEna_7):
            {
                PAL_ASSERT(pMetadata->hasEntry.cullDistEna_7 == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.cullDistEna_7 = value;
                }

                pMetadata->hasEntry.cullDistEna_7 = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxPointSize):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxPointSize == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxPointSize = value;
                }

                pMetadata->hasEntry.useVtxPointSize = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxEdgeFlag):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxEdgeFlag == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxEdgeFlag = value;
                }

                pMetadata->hasEntry.useVtxEdgeFlag = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxRenderTargetIndx):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxRenderTargetIndx == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxRenderTargetIndx = value;
                }

                pMetadata->hasEntry.useVtxRenderTargetIndx = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxViewportIndx):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxViewportIndx == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxViewportIndx = value;
                }

                pMetadata->hasEntry.useVtxViewportIndx = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxKillFlag):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxKillFlag == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxKillFlag = value;
                }

                pMetadata->hasEntry.useVtxKillFlag = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::VsOutMiscVecEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsOutMiscVecEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsOutMiscVecEna = value;
                }

                pMetadata->hasEntry.vsOutMiscVecEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::VsOutCcDist0VecEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsOutCcDist0VecEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsOutCcDist0VecEna = value;
                }

                pMetadata->hasEntry.vsOutCcDist0VecEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::VsOutCcDist1VecEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsOutCcDist1VecEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsOutCcDist1VecEna = value;
                }

                pMetadata->hasEntry.vsOutCcDist1VecEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::VsOutMiscSideBusEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsOutMiscSideBusEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsOutMiscSideBusEna = value;
                }

                pMetadata->hasEntry.vsOutMiscSideBusEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxLineWidth):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxLineWidth == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxLineWidth = value;
                }

                pMetadata->hasEntry.useVtxLineWidth = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxVrsRate):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxVrsRate == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxVrsRate = value;
                }

                pMetadata->hasEntry.useVtxVrsRate = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::BypassVtxRateCombiner):
            {
                PAL_ASSERT(pMetadata->hasEntry.bypassVtxRateCombiner == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.bypassVtxRateCombiner = value;
                }

                pMetadata->hasEntry.bypassVtxRateCombiner = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::BypassPrimRateCombiner):
            {
                PAL_ASSERT(pMetadata->hasEntry.bypassPrimRateCombiner == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.bypassPrimRateCombiner = value;
                }

                pMetadata->hasEntry.bypassPrimRateCombiner = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxGsCutFlag):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxGsCutFlag == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxGsCutFlag = value;
                }

                pMetadata->hasEntry.useVtxGsCutFlag = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaClVsOutCntlMetadataKey::UseVtxFsrSelect):
            {
                PAL_ASSERT(pMetadata->hasEntry.useVtxFsrSelect == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useVtxFsrSelect = value;
                }

                pMetadata->hasEntry.useVtxFsrSelect = (result == Result::Success);
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
inline Result DeserializeSpiVsOutConfigMetadata(
    MsgPackReader*  pReader,
    SpiVsOutConfigMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiVsOutConfigMetadataKey::NoPcExport):
            {
                PAL_ASSERT(pMetadata->hasEntry.noPcExport == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.noPcExport = value;
                }

                pMetadata->hasEntry.noPcExport = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiVsOutConfigMetadataKey::VsExportCount):
                PAL_ASSERT(pMetadata->hasEntry.vsExportCount == 0);
                result = pReader->UnpackNext(&pMetadata->vsExportCount);
                pMetadata->hasEntry.vsExportCount = (result == Result::Success);;
                break;

            case HashLiteralString(SpiVsOutConfigMetadataKey::PrimExportCount):
                PAL_ASSERT(pMetadata->hasEntry.primExportCount == 0);
                result = pReader->UnpackNext(&pMetadata->primExportCount);
                pMetadata->hasEntry.primExportCount = (result == Result::Success);;
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
inline Result DeserializeVgtStrmoutConfigMetadata(
    MsgPackReader*  pReader,
    VgtStrmoutConfigMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtStrmoutConfigMetadataKey::Streamout_0En):
            {
                PAL_ASSERT(pMetadata->hasEntry.streamout_0En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.streamout_0En = value;
                }

                pMetadata->hasEntry.streamout_0En = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtStrmoutConfigMetadataKey::Streamout_1En):
            {
                PAL_ASSERT(pMetadata->hasEntry.streamout_1En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.streamout_1En = value;
                }

                pMetadata->hasEntry.streamout_1En = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtStrmoutConfigMetadataKey::Streamout_2En):
            {
                PAL_ASSERT(pMetadata->hasEntry.streamout_2En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.streamout_2En = value;
                }

                pMetadata->hasEntry.streamout_2En = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtStrmoutConfigMetadataKey::Streamout_3En):
            {
                PAL_ASSERT(pMetadata->hasEntry.streamout_3En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.streamout_3En = value;
                }

                pMetadata->hasEntry.streamout_3En = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtStrmoutConfigMetadataKey::RastStream):
                PAL_ASSERT(pMetadata->hasEntry.rastStream == 0);
                result = pReader->UnpackNext(&pMetadata->rastStream);
                pMetadata->hasEntry.rastStream = (result == Result::Success);;
                break;

            case HashLiteralString(VgtStrmoutConfigMetadataKey::PrimsNeededCntEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.primsNeededCntEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.primsNeededCntEn = value;
                }

                pMetadata->hasEntry.primsNeededCntEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(VgtStrmoutConfigMetadataKey::RastStreamMask):
                PAL_ASSERT(pMetadata->hasEntry.rastStreamMask == 0);
                result = pReader->UnpackNext(&pMetadata->rastStreamMask);
                pMetadata->hasEntry.rastStreamMask = (result == Result::Success);;
                break;

            case HashLiteralString(VgtStrmoutConfigMetadataKey::UseRastStreamMask):
            {
                PAL_ASSERT(pMetadata->hasEntry.useRastStreamMask == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.useRastStreamMask = value;
                }

                pMetadata->hasEntry.useRastStreamMask = (result == Result::Success);
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
inline Result DeserializeVgtStrmoutBufferConfigMetadata(
    MsgPackReader*  pReader,
    VgtStrmoutBufferConfigMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(VgtStrmoutBufferConfigMetadataKey::Stream_0BufferEn):
                PAL_ASSERT(pMetadata->hasEntry.stream_0BufferEn == 0);
                result = pReader->UnpackNext(&pMetadata->stream_0BufferEn);
                pMetadata->hasEntry.stream_0BufferEn = (result == Result::Success);;
                break;

            case HashLiteralString(VgtStrmoutBufferConfigMetadataKey::Stream_1BufferEn):
                PAL_ASSERT(pMetadata->hasEntry.stream_1BufferEn == 0);
                result = pReader->UnpackNext(&pMetadata->stream_1BufferEn);
                pMetadata->hasEntry.stream_1BufferEn = (result == Result::Success);;
                break;

            case HashLiteralString(VgtStrmoutBufferConfigMetadataKey::Stream_2BufferEn):
                PAL_ASSERT(pMetadata->hasEntry.stream_2BufferEn == 0);
                result = pReader->UnpackNext(&pMetadata->stream_2BufferEn);
                pMetadata->hasEntry.stream_2BufferEn = (result == Result::Success);;
                break;

            case HashLiteralString(VgtStrmoutBufferConfigMetadataKey::Stream_3BufferEn):
                PAL_ASSERT(pMetadata->hasEntry.stream_3BufferEn == 0);
                result = pReader->UnpackNext(&pMetadata->stream_3BufferEn);
                pMetadata->hasEntry.stream_3BufferEn = (result == Result::Success);;
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
inline Result DeserializeCbShaderMaskMetadata(
    MsgPackReader*  pReader,
    CbShaderMaskMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(CbShaderMaskMetadataKey::Output0Enable):
                PAL_ASSERT(pMetadata->hasEntry.output0Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output0Enable);
                pMetadata->hasEntry.output0Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output1Enable):
                PAL_ASSERT(pMetadata->hasEntry.output1Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output1Enable);
                pMetadata->hasEntry.output1Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output2Enable):
                PAL_ASSERT(pMetadata->hasEntry.output2Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output2Enable);
                pMetadata->hasEntry.output2Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output3Enable):
                PAL_ASSERT(pMetadata->hasEntry.output3Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output3Enable);
                pMetadata->hasEntry.output3Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output4Enable):
                PAL_ASSERT(pMetadata->hasEntry.output4Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output4Enable);
                pMetadata->hasEntry.output4Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output5Enable):
                PAL_ASSERT(pMetadata->hasEntry.output5Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output5Enable);
                pMetadata->hasEntry.output5Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output6Enable):
                PAL_ASSERT(pMetadata->hasEntry.output6Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output6Enable);
                pMetadata->hasEntry.output6Enable = (result == Result::Success);;
                break;

            case HashLiteralString(CbShaderMaskMetadataKey::Output7Enable):
                PAL_ASSERT(pMetadata->hasEntry.output7Enable == 0);
                result = pReader->UnpackNext(&pMetadata->output7Enable);
                pMetadata->hasEntry.output7Enable = (result == Result::Success);;
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
inline Result DeserializeDbShaderControlMetadata(
    MsgPackReader*  pReader,
    DbShaderControlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(DbShaderControlMetadataKey::ZExportEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.zExportEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.zExportEnable = value;
                }

                pMetadata->hasEntry.zExportEnable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::StencilTestValExportEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.stencilTestValExportEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.stencilTestValExportEnable = value;
                }

                pMetadata->hasEntry.stencilTestValExportEnable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::StencilOpValExportEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.stencilOpValExportEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.stencilOpValExportEnable = value;
                }

                pMetadata->hasEntry.stencilOpValExportEnable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::ZOrder):
                PAL_ASSERT(pMetadata->hasEntry.zOrder == 0);
                result = pReader->UnpackNext(&pMetadata->zOrder);
                pMetadata->hasEntry.zOrder = (result == Result::Success);;
                break;

            case HashLiteralString(DbShaderControlMetadataKey::KillEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.killEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.killEnable = value;
                }

                pMetadata->hasEntry.killEnable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::CoverageToMaskEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.coverageToMaskEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.coverageToMaskEn = value;
                }

                pMetadata->hasEntry.coverageToMaskEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::MaskExportEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.maskExportEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.maskExportEnable = value;
                }

                pMetadata->hasEntry.maskExportEnable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::ExecOnHierFail):
            {
                PAL_ASSERT(pMetadata->hasEntry.execOnHierFail == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.execOnHierFail = value;
                }

                pMetadata->hasEntry.execOnHierFail = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::ExecOnNoop):
            {
                PAL_ASSERT(pMetadata->hasEntry.execOnNoop == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.execOnNoop = value;
                }

                pMetadata->hasEntry.execOnNoop = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::AlphaToMaskDisable):
            {
                PAL_ASSERT(pMetadata->hasEntry.alphaToMaskDisable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.alphaToMaskDisable = value;
                }

                pMetadata->hasEntry.alphaToMaskDisable = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::DepthBeforeShader):
            {
                PAL_ASSERT(pMetadata->hasEntry.depthBeforeShader == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.depthBeforeShader = value;
                }

                pMetadata->hasEntry.depthBeforeShader = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::ConservativeZExport):
                PAL_ASSERT(pMetadata->hasEntry.conservativeZExport == 0);
                result = pReader->UnpackNext(&pMetadata->conservativeZExport);
                pMetadata->hasEntry.conservativeZExport = (result == Result::Success);;
                break;

            case HashLiteralString(DbShaderControlMetadataKey::PrimitiveOrderedPixelShader):
            {
                PAL_ASSERT(pMetadata->hasEntry.primitiveOrderedPixelShader == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.primitiveOrderedPixelShader = value;
                }

                pMetadata->hasEntry.primitiveOrderedPixelShader = (result == Result::Success);
                break;
            }

            case HashLiteralString(DbShaderControlMetadataKey::PreShaderDepthCoverageEnable):
            {
                PAL_ASSERT(pMetadata->hasEntry.preShaderDepthCoverageEnable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.preShaderDepthCoverageEnable = value;
                }

                pMetadata->hasEntry.preShaderDepthCoverageEnable = (result == Result::Success);
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
inline Result DeserializeSpiPsInControlMetadata(
    MsgPackReader*  pReader,
    SpiPsInControlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiPsInControlMetadataKey::NumInterps):
                PAL_ASSERT(pMetadata->hasEntry.numInterps == 0);
                result = pReader->UnpackNext(&pMetadata->numInterps);
                pMetadata->hasEntry.numInterps = (result == Result::Success);;
                break;

            case HashLiteralString(SpiPsInControlMetadataKey::ParamGen):
            {
                PAL_ASSERT(pMetadata->hasEntry.paramGen == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.paramGen = value;
                }

                pMetadata->hasEntry.paramGen = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInControlMetadataKey::OffchipParamEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.offchipParamEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.offchipParamEn = value;
                }

                pMetadata->hasEntry.offchipParamEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInControlMetadataKey::LatePcDealloc):
            {
                PAL_ASSERT(pMetadata->hasEntry.latePcDealloc == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.latePcDealloc = value;
                }

                pMetadata->hasEntry.latePcDealloc = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInControlMetadataKey::NumPrimInterp):
                PAL_ASSERT(pMetadata->hasEntry.numPrimInterp == 0);
                result = pReader->UnpackNext(&pMetadata->numPrimInterp);
                pMetadata->hasEntry.numPrimInterp = (result == Result::Success);;
                break;

            case HashLiteralString(SpiPsInControlMetadataKey::BcOptimizeDisable):
            {
                PAL_ASSERT(pMetadata->hasEntry.bcOptimizeDisable == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.bcOptimizeDisable = value;
                }

                pMetadata->hasEntry.bcOptimizeDisable = (result == Result::Success);
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
inline Result DeserializePaScShaderControlMetadata(
    MsgPackReader*  pReader,
    PaScShaderControlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PaScShaderControlMetadataKey::LoadCollisionWaveid):
            {
                PAL_ASSERT(pMetadata->hasEntry.loadCollisionWaveid == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.loadCollisionWaveid = value;
                }

                pMetadata->hasEntry.loadCollisionWaveid = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaScShaderControlMetadataKey::LoadIntrawaveCollision):
            {
                PAL_ASSERT(pMetadata->hasEntry.loadIntrawaveCollision == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.loadIntrawaveCollision = value;
                }

                pMetadata->hasEntry.loadIntrawaveCollision = (result == Result::Success);
                break;
            }

            case HashLiteralString(PaScShaderControlMetadataKey::WaveBreakRegionSize):
                PAL_ASSERT(pMetadata->hasEntry.waveBreakRegionSize == 0);
                result = pReader->UnpackNext(&pMetadata->waveBreakRegionSize);
                pMetadata->hasEntry.waveBreakRegionSize = (result == Result::Success);;
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
inline Result DeserializeSpiBarycCntlMetadata(
    MsgPackReader*  pReader,
    SpiBarycCntlMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiBarycCntlMetadataKey::PosFloatLocation):
                PAL_ASSERT(pMetadata->hasEntry.posFloatLocation == 0);
                result = pReader->UnpackNext(&pMetadata->posFloatLocation);
                pMetadata->hasEntry.posFloatLocation = (result == Result::Success);;
                break;

            case HashLiteralString(SpiBarycCntlMetadataKey::FrontFaceAllBits):
            {
                PAL_ASSERT(pMetadata->hasEntry.frontFaceAllBits == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.frontFaceAllBits = value;
                }

                pMetadata->hasEntry.frontFaceAllBits = (result == Result::Success);
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
inline Result DeserializeSpiPsInputEnaMetadata(
    MsgPackReader*  pReader,
    SpiPsInputEnaMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiPsInputEnaMetadataKey::PerspSampleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspSampleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspSampleEna = value;
                }

                pMetadata->hasEntry.perspSampleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PerspCenterEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspCenterEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspCenterEna = value;
                }

                pMetadata->hasEntry.perspCenterEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PerspCentroidEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspCentroidEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspCentroidEna = value;
                }

                pMetadata->hasEntry.perspCentroidEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PerspPullModelEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspPullModelEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspPullModelEna = value;
                }

                pMetadata->hasEntry.perspPullModelEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::LinearSampleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearSampleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearSampleEna = value;
                }

                pMetadata->hasEntry.linearSampleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::LinearCenterEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearCenterEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearCenterEna = value;
                }

                pMetadata->hasEntry.linearCenterEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::LinearCentroidEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearCentroidEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearCentroidEna = value;
                }

                pMetadata->hasEntry.linearCentroidEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::LineStippleTexEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.lineStippleTexEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.lineStippleTexEna = value;
                }

                pMetadata->hasEntry.lineStippleTexEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PosXFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posXFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posXFloatEna = value;
                }

                pMetadata->hasEntry.posXFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PosYFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posYFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posYFloatEna = value;
                }

                pMetadata->hasEntry.posYFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PosZFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posZFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posZFloatEna = value;
                }

                pMetadata->hasEntry.posZFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PosWFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posWFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posWFloatEna = value;
                }

                pMetadata->hasEntry.posWFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::FrontFaceEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.frontFaceEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.frontFaceEna = value;
                }

                pMetadata->hasEntry.frontFaceEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::AncillaryEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.ancillaryEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.ancillaryEna = value;
                }

                pMetadata->hasEntry.ancillaryEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::SampleCoverageEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.sampleCoverageEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.sampleCoverageEna = value;
                }

                pMetadata->hasEntry.sampleCoverageEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputEnaMetadataKey::PosFixedPtEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posFixedPtEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posFixedPtEna = value;
                }

                pMetadata->hasEntry.posFixedPtEna = (result == Result::Success);
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
inline Result DeserializeSpiPsInputAddrMetadata(
    MsgPackReader*  pReader,
    SpiPsInputAddrMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiPsInputAddrMetadataKey::PerspSampleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspSampleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspSampleEna = value;
                }

                pMetadata->hasEntry.perspSampleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PerspCenterEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspCenterEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspCenterEna = value;
                }

                pMetadata->hasEntry.perspCenterEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PerspCentroidEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspCentroidEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspCentroidEna = value;
                }

                pMetadata->hasEntry.perspCentroidEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PerspPullModelEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.perspPullModelEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.perspPullModelEna = value;
                }

                pMetadata->hasEntry.perspPullModelEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::LinearSampleEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearSampleEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearSampleEna = value;
                }

                pMetadata->hasEntry.linearSampleEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::LinearCenterEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearCenterEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearCenterEna = value;
                }

                pMetadata->hasEntry.linearCenterEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::LinearCentroidEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.linearCentroidEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.linearCentroidEna = value;
                }

                pMetadata->hasEntry.linearCentroidEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::LineStippleTexEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.lineStippleTexEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.lineStippleTexEna = value;
                }

                pMetadata->hasEntry.lineStippleTexEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PosXFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posXFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posXFloatEna = value;
                }

                pMetadata->hasEntry.posXFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PosYFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posYFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posYFloatEna = value;
                }

                pMetadata->hasEntry.posYFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PosZFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posZFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posZFloatEna = value;
                }

                pMetadata->hasEntry.posZFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PosWFloatEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posWFloatEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posWFloatEna = value;
                }

                pMetadata->hasEntry.posWFloatEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::FrontFaceEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.frontFaceEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.frontFaceEna = value;
                }

                pMetadata->hasEntry.frontFaceEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::AncillaryEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.ancillaryEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.ancillaryEna = value;
                }

                pMetadata->hasEntry.ancillaryEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::SampleCoverageEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.sampleCoverageEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.sampleCoverageEna = value;
                }

                pMetadata->hasEntry.sampleCoverageEna = (result == Result::Success);
                break;
            }

            case HashLiteralString(SpiPsInputAddrMetadataKey::PosFixedPtEna):
            {
                PAL_ASSERT(pMetadata->hasEntry.posFixedPtEna == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.posFixedPtEna = value;
                }

                pMetadata->hasEntry.posFixedPtEna = (result == Result::Success);
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
inline Result DeserializeSpiShaderColFormatMetadata(
    MsgPackReader*  pReader,
    SpiShaderColFormatMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_0ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_0ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_0ExportFormat);
                pMetadata->hasEntry.col_0ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_1ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_1ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_1ExportFormat);
                pMetadata->hasEntry.col_1ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_2ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_2ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_2ExportFormat);
                pMetadata->hasEntry.col_2ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_3ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_3ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_3ExportFormat);
                pMetadata->hasEntry.col_3ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_4ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_4ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_4ExportFormat);
                pMetadata->hasEntry.col_4ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_5ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_5ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_5ExportFormat);
                pMetadata->hasEntry.col_5ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_6ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_6ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_6ExportFormat);
                pMetadata->hasEntry.col_6ExportFormat = (result == Result::Success);;
                break;

            case HashLiteralString(SpiShaderColFormatMetadataKey::Col_7ExportFormat):
                PAL_ASSERT(pMetadata->hasEntry.col_7ExportFormat == 0);
                result = pReader->UnpackNext(&pMetadata->col_7ExportFormat);
                pMetadata->hasEntry.col_7ExportFormat = (result == Result::Success);;
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
inline Result DeserializeGraphicsRegisterMetadata(
    MsgPackReader*  pReader,
    GraphicsRegisterMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(GraphicsRegisterMetadataKey::NggCullingDataReg):
                PAL_ASSERT(pMetadata->hasEntry.nggCullingDataReg == 0);
                result = pReader->UnpackNext(&pMetadata->nggCullingDataReg);
                pMetadata->hasEntry.nggCullingDataReg = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::LsVgprCompCnt):
                PAL_ASSERT(pMetadata->hasEntry.lsVgprCompCnt == 0);
                result = pReader->UnpackNext(&pMetadata->lsVgprCompCnt);
                pMetadata->hasEntry.lsVgprCompCnt = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::HsTgSizeEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.hsTgSizeEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.hsTgSizeEn = value;
                }

                pMetadata->hasEntry.hsTgSizeEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::EsVgprCompCnt):
                PAL_ASSERT(pMetadata->hasEntry.esVgprCompCnt == 0);
                result = pReader->UnpackNext(&pMetadata->esVgprCompCnt);
                pMetadata->hasEntry.esVgprCompCnt = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::GsVgprCompCnt):
                PAL_ASSERT(pMetadata->hasEntry.gsVgprCompCnt == 0);
                result = pReader->UnpackNext(&pMetadata->gsVgprCompCnt);
                pMetadata->hasEntry.gsVgprCompCnt = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VsVgprCompCnt):
                PAL_ASSERT(pMetadata->hasEntry.vsVgprCompCnt == 0);
                result = pReader->UnpackNext(&pMetadata->vsVgprCompCnt);
                pMetadata->hasEntry.vsVgprCompCnt = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VsSoBase0En):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsSoBase0En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsSoBase0En = value;
                }

                pMetadata->hasEntry.vsSoBase0En = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VsSoBase1En):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsSoBase1En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsSoBase1En = value;
                }

                pMetadata->hasEntry.vsSoBase1En = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VsSoBase2En):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsSoBase2En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsSoBase2En = value;
                }

                pMetadata->hasEntry.vsSoBase2En = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VsSoBase3En):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsSoBase3En == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsSoBase3En = value;
                }

                pMetadata->hasEntry.vsSoBase3En = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VsStreamoutEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsStreamoutEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsStreamoutEn = value;
                }

                pMetadata->hasEntry.vsStreamoutEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VsPcBaseEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.vsPcBaseEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vsPcBaseEn = value;
                }

                pMetadata->hasEntry.vsPcBaseEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::PsLoadProvokingVtx):
            {
                PAL_ASSERT(pMetadata->hasEntry.psLoadProvokingVtx == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.psLoadProvokingVtx = value;
                }

                pMetadata->hasEntry.psLoadProvokingVtx = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::PsWaveCntEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.psWaveCntEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.psWaveCntEn = value;
                }

                pMetadata->hasEntry.psWaveCntEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::PsExtraLdsSize):
                PAL_ASSERT(pMetadata->hasEntry.psExtraLdsSize == 0);
                result = pReader->UnpackNext(&pMetadata->psExtraLdsSize);
                pMetadata->hasEntry.psExtraLdsSize = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PaClClipCntl):
                PAL_ASSERT(pMetadata->hasEntry.paClClipCntl == 0);
                pReader->Next();
                result = DeserializePaClClipCntlMetadata(
                        pReader, &pMetadata->paClClipCntl);
                    pMetadata->hasEntry.paClClipCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PaClVteCntl):
                PAL_ASSERT(pMetadata->hasEntry.paClVteCntl == 0);
                pReader->Next();
                result = DeserializePaClVteCntlMetadata(
                        pReader, &pMetadata->paClVteCntl);
                    pMetadata->hasEntry.paClVteCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PaSuVtxCntl):
                PAL_ASSERT(pMetadata->hasEntry.paSuVtxCntl == 0);
                pReader->Next();
                result = DeserializePaSuVtxCntlMetadata(
                        pReader, &pMetadata->paSuVtxCntl);
                    pMetadata->hasEntry.paSuVtxCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PsIterSample):
            {
                PAL_ASSERT(pMetadata->hasEntry.psIterSample == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.psIterSample = value;
                }

                pMetadata->hasEntry.psIterSample = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtShaderStagesEn):
                PAL_ASSERT(pMetadata->hasEntry.vgtShaderStagesEn == 0);
                pReader->Next();
                result = DeserializeVgtShaderStagesEnMetadata(
                        pReader, &pMetadata->vgtShaderStagesEn);
                    pMetadata->hasEntry.vgtShaderStagesEn = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtReuseOff):
            {
                PAL_ASSERT(pMetadata->hasEntry.vgtReuseOff == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vgtReuseOff = value;
                }

                pMetadata->hasEntry.vgtReuseOff = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsMode):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsMode == 0);
                pReader->Next();
                result = DeserializeVgtGsModeMetadata(
                        pReader, &pMetadata->vgtGsMode);
                    pMetadata->hasEntry.vgtGsMode = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtTfParam):
                PAL_ASSERT(pMetadata->hasEntry.vgtTfParam == 0);
                pReader->Next();
                result = DeserializeVgtTfParamMetadata(
                        pReader, &pMetadata->vgtTfParam);
                    pMetadata->hasEntry.vgtTfParam = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtLsHsConfig):
                PAL_ASSERT(pMetadata->hasEntry.vgtLsHsConfig == 0);
                pReader->Next();
                result = DeserializeVgtLsHsConfigMetadata(
                        pReader, &pMetadata->vgtLsHsConfig);
                    pMetadata->hasEntry.vgtLsHsConfig = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::IaMultiVgtParam):
                PAL_ASSERT(pMetadata->hasEntry.iaMultiVgtParam == 0);
                pReader->Next();
                result = DeserializeIaMultiVgtParamMetadata(
                        pReader, &pMetadata->iaMultiVgtParam);
                    pMetadata->hasEntry.iaMultiVgtParam = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiInterpControl):
                PAL_ASSERT(pMetadata->hasEntry.spiInterpControl == 0);
                pReader->Next();
                result = DeserializeSpiInterpControlMetadata(
                        pReader, &pMetadata->spiInterpControl);
                    pMetadata->hasEntry.spiInterpControl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiPsInputCntl):
                PAL_ASSERT(pMetadata->hasEntry.spiPsInputCntl == 0);
                pReader->Next();
                result = DeserializeSpiPsInputCntlMetadata(
                        pReader, &pMetadata->spiPsInputCntl[0]);
                    pMetadata->hasEntry.spiPsInputCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtHosMinTessLevel):
                PAL_ASSERT(pMetadata->hasEntry.vgtHosMinTessLevel == 0);
                result = pReader->UnpackNext(&pMetadata->vgtHosMinTessLevel);
                pMetadata->hasEntry.vgtHosMinTessLevel = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtHosMaxTessLevel):
                PAL_ASSERT(pMetadata->hasEntry.vgtHosMaxTessLevel == 0);
                result = pReader->UnpackNext(&pMetadata->vgtHosMaxTessLevel);
                pMetadata->hasEntry.vgtHosMaxTessLevel = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderGsMeshletDim):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderGsMeshletDim == 0);
                pReader->Next();
                result = DeserializeSpiShaderGsMeshletDimMetadata(
                        pReader, &pMetadata->spiShaderGsMeshletDim);
                    pMetadata->hasEntry.spiShaderGsMeshletDim = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderGsMeshletExpAlloc):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderGsMeshletExpAlloc == 0);
                pReader->Next();
                result = DeserializeSpiShaderGsMeshletExpAllocMetadata(
                        pReader, &pMetadata->spiShaderGsMeshletExpAlloc);
                    pMetadata->hasEntry.spiShaderGsMeshletExpAlloc = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::MeshLinearDispatchFromTask):
            {
                PAL_ASSERT(pMetadata->hasEntry.meshLinearDispatchFromTask == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.meshLinearDispatchFromTask = value;
                }

                pMetadata->hasEntry.meshLinearDispatchFromTask = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsMaxVertOut):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsMaxVertOut == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsMaxVertOut);
                pMetadata->hasEntry.vgtGsMaxVertOut = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsInstanceCnt):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsInstanceCnt == 0);
                pReader->Next();
                result = DeserializeVgtGsInstanceCntMetadata(
                        pReader, &pMetadata->vgtGsInstanceCnt);
                    pMetadata->hasEntry.vgtGsInstanceCnt = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtEsgsRingItemsize):
                PAL_ASSERT(pMetadata->hasEntry.vgtEsgsRingItemsize == 0);
                result = pReader->UnpackNext(&pMetadata->vgtEsgsRingItemsize);
                pMetadata->hasEntry.vgtEsgsRingItemsize = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtDrawPrimPayloadEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.vgtDrawPrimPayloadEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vgtDrawPrimPayloadEn = value;
                }

                pMetadata->hasEntry.vgtDrawPrimPayloadEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsOutPrimType):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsOutPrimType == 0);
                pReader->Next();
                result = DeserializeVgtGsOutPrimTypeMetadata(
                        pReader, &pMetadata->vgtGsOutPrimType);
                    pMetadata->hasEntry.vgtGsOutPrimType = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsVertItemsize):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsVertItemsize == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsVertItemsize);
                pMetadata->hasEntry.vgtGsVertItemsize = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsvsRingOffset):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsvsRingOffset == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsvsRingOffset);
                pMetadata->hasEntry.vgtGsvsRingOffset = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsvsRingItemsize):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsvsRingItemsize == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsvsRingItemsize);
                pMetadata->hasEntry.vgtGsvsRingItemsize = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtEsPerGs):
                PAL_ASSERT(pMetadata->hasEntry.vgtEsPerGs == 0);
                result = pReader->UnpackNext(&pMetadata->vgtEsPerGs);
                pMetadata->hasEntry.vgtEsPerGs = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsPerEs):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsPerEs == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsPerEs);
                pMetadata->hasEntry.vgtGsPerEs = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsPerVs):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsPerVs == 0);
                result = pReader->UnpackNext(&pMetadata->vgtGsPerVs);
                pMetadata->hasEntry.vgtGsPerVs = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::MaxVertsPerSubgroup):
                PAL_ASSERT(pMetadata->hasEntry.maxVertsPerSubgroup == 0);
                result = pReader->UnpackNext(&pMetadata->maxVertsPerSubgroup);
                pMetadata->hasEntry.maxVertsPerSubgroup = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderIdxFormat):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderIdxFormat == 0);
                result = pReader->UnpackNext(&pMetadata->spiShaderIdxFormat);
                pMetadata->hasEntry.spiShaderIdxFormat = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::GeNggSubgrpCntl):
                PAL_ASSERT(pMetadata->hasEntry.geNggSubgrpCntl == 0);
                pReader->Next();
                result = DeserializeGeNggSubgrpCntlMetadata(
                        pReader, &pMetadata->geNggSubgrpCntl);
                    pMetadata->hasEntry.geNggSubgrpCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtGsOnchipCntl):
                PAL_ASSERT(pMetadata->hasEntry.vgtGsOnchipCntl == 0);
                pReader->Next();
                result = DeserializeVgtGsOnchipCntlMetadata(
                        pReader, &pMetadata->vgtGsOnchipCntl);
                    pMetadata->hasEntry.vgtGsOnchipCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PaClVsOutCntl):
                PAL_ASSERT(pMetadata->hasEntry.paClVsOutCntl == 0);
                pReader->Next();
                result = DeserializePaClVsOutCntlMetadata(
                        pReader, &pMetadata->paClVsOutCntl);
                    pMetadata->hasEntry.paClVsOutCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderPosFormat):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderPosFormat == 0);
                result = pReader->UnpackNext(&pMetadata->spiShaderPosFormat);
                pMetadata->hasEntry.spiShaderPosFormat = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiVsOutConfig):
                PAL_ASSERT(pMetadata->hasEntry.spiVsOutConfig == 0);
                pReader->Next();
                result = DeserializeSpiVsOutConfigMetadata(
                        pReader, &pMetadata->spiVsOutConfig);
                    pMetadata->hasEntry.spiVsOutConfig = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtPrimitiveIdEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.vgtPrimitiveIdEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.vgtPrimitiveIdEn = value;
                }

                pMetadata->hasEntry.vgtPrimitiveIdEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::NggDisableProvokReuse):
            {
                PAL_ASSERT(pMetadata->hasEntry.nggDisableProvokReuse == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.nggDisableProvokReuse = value;
                }

                pMetadata->hasEntry.nggDisableProvokReuse = (result == Result::Success);
                break;
            }

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtStrmoutConfig):
                PAL_ASSERT(pMetadata->hasEntry.vgtStrmoutConfig == 0);
                pReader->Next();
                result = DeserializeVgtStrmoutConfigMetadata(
                        pReader, &pMetadata->vgtStrmoutConfig);
                    pMetadata->hasEntry.vgtStrmoutConfig = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::VgtStrmoutBufferConfig):
                PAL_ASSERT(pMetadata->hasEntry.vgtStrmoutBufferConfig == 0);
                pReader->Next();
                result = DeserializeVgtStrmoutBufferConfigMetadata(
                        pReader, &pMetadata->vgtStrmoutBufferConfig);
                    pMetadata->hasEntry.vgtStrmoutBufferConfig = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::CbShaderMask):
                PAL_ASSERT(pMetadata->hasEntry.cbShaderMask == 0);
                pReader->Next();
                result = DeserializeCbShaderMaskMetadata(
                        pReader, &pMetadata->cbShaderMask);
                    pMetadata->hasEntry.cbShaderMask = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::DbShaderControl):
                PAL_ASSERT(pMetadata->hasEntry.dbShaderControl == 0);
                pReader->Next();
                result = DeserializeDbShaderControlMetadata(
                        pReader, &pMetadata->dbShaderControl);
                    pMetadata->hasEntry.dbShaderControl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiPsInControl):
                PAL_ASSERT(pMetadata->hasEntry.spiPsInControl == 0);
                pReader->Next();
                result = DeserializeSpiPsInControlMetadata(
                        pReader, &pMetadata->spiPsInControl);
                    pMetadata->hasEntry.spiPsInControl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::AaCoverageToShaderSelect):
                PAL_ASSERT(pMetadata->hasEntry.aaCoverageToShaderSelect == 0);
                result = DeserializeEnum(pReader, &pMetadata->aaCoverageToShaderSelect);
                pMetadata->hasEntry.aaCoverageToShaderSelect = (result == Result::Success);;
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::PaScShaderControl):
                PAL_ASSERT(pMetadata->hasEntry.paScShaderControl == 0);
                pReader->Next();
                result = DeserializePaScShaderControlMetadata(
                        pReader, &pMetadata->paScShaderControl);
                    pMetadata->hasEntry.paScShaderControl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiBarycCntl):
                PAL_ASSERT(pMetadata->hasEntry.spiBarycCntl == 0);
                pReader->Next();
                result = DeserializeSpiBarycCntlMetadata(
                        pReader, &pMetadata->spiBarycCntl);
                    pMetadata->hasEntry.spiBarycCntl = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiPsInputEna):
                PAL_ASSERT(pMetadata->hasEntry.spiPsInputEna == 0);
                pReader->Next();
                result = DeserializeSpiPsInputEnaMetadata(
                        pReader, &pMetadata->spiPsInputEna);
                    pMetadata->hasEntry.spiPsInputEna = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiPsInputAddr):
                PAL_ASSERT(pMetadata->hasEntry.spiPsInputAddr == 0);
                pReader->Next();
                result = DeserializeSpiPsInputAddrMetadata(
                        pReader, &pMetadata->spiPsInputAddr);
                    pMetadata->hasEntry.spiPsInputAddr = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderColFormat):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderColFormat == 0);
                pReader->Next();
                result = DeserializeSpiShaderColFormatMetadata(
                        pReader, &pMetadata->spiShaderColFormat);
                    pMetadata->hasEntry.spiShaderColFormat = (result == Result::Success);
                break;

            case HashLiteralString(GraphicsRegisterMetadataKey::SpiShaderZFormat):
                PAL_ASSERT(pMetadata->hasEntry.spiShaderZFormat == 0);
                result = pReader->UnpackNext(&pMetadata->spiShaderZFormat);
                pMetadata->hasEntry.spiShaderZFormat = (result == Result::Success);;
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
inline Result DeserializeComputeRegisterMetadata(
    MsgPackReader*  pReader,
    ComputeRegisterMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(ComputeRegisterMetadataKey::TgidXEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.tgidXEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.tgidXEn = value;
                }

                pMetadata->hasEntry.tgidXEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(ComputeRegisterMetadataKey::TgidYEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.tgidYEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.tgidYEn = value;
                }

                pMetadata->hasEntry.tgidYEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(ComputeRegisterMetadataKey::TgidZEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.tgidZEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.tgidZEn = value;
                }

                pMetadata->hasEntry.tgidZEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(ComputeRegisterMetadataKey::TgSizeEn):
            {
                PAL_ASSERT(pMetadata->hasEntry.tgSizeEn == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.tgSizeEn = value;
                }

                pMetadata->hasEntry.tgSizeEn = (result == Result::Success);
                break;
            }

            case HashLiteralString(ComputeRegisterMetadataKey::TidigCompCnt):
                PAL_ASSERT(pMetadata->hasEntry.tidigCompCnt == 0);
                result = pReader->UnpackNext(&pMetadata->tidigCompCnt);
                pMetadata->hasEntry.tidigCompCnt = (result == Result::Success);;
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
inline Result DeserializePipelineMetadata(
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
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(PipelineMetadataKey::Name):
                PAL_ASSERT(pMetadata->hasEntry.name == 0);
                result = pReader->UnpackNext(&pMetadata->name);
                pMetadata->hasEntry.name = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::Type):
                PAL_ASSERT(pMetadata->hasEntry.type == 0);
                result = DeserializeEnum(pReader, &pMetadata->type);
                pMetadata->hasEntry.type = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::InternalPipelineHash):
                PAL_ASSERT(pMetadata->hasEntry.internalPipelineHash == 0);
                result = pReader->UnpackNext(&pMetadata->internalPipelineHash);
                pMetadata->hasEntry.internalPipelineHash = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::ResourceHash):
                PAL_ASSERT(pMetadata->hasEntry.resourceHash == 0);
                result = pReader->UnpackNext(&pMetadata->resourceHash);
                pMetadata->hasEntry.resourceHash = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::Shaders):
                PAL_ASSERT(pMetadata->hasEntry.shader == 0);
                pReader->Next();
                result = DeserializeShaderMetadata(
                        pReader, &pMetadata->shader);
                    pMetadata->hasEntry.shader = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::HardwareStages):
                PAL_ASSERT(pMetadata->hasEntry.hardwareStage == 0);
                pReader->Next();
                result = DeserializeHardwareStageMetadata(
                        pReader, &pMetadata->hardwareStage);
                    pMetadata->hasEntry.hardwareStage = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::ShaderFunctions):
                PAL_ASSERT(pMetadata->hasEntry.shaderFunctions == 0);
                pMetadata->shaderFunctions = pReader->Tell();
                pMetadata->hasEntry.shaderFunctions = (result == Result::Success);;
                result = pReader->Skip(1);
                break;

            case HashLiteralString(PipelineMetadataKey::Registers):
                PAL_ASSERT(pMetadata->hasEntry.registers == 0);
                pMetadata->registers = pReader->Tell();
                pMetadata->hasEntry.registers = (result == Result::Success);;
                result = pReader->Skip(1);
                break;

            case HashLiteralString(PipelineMetadataKey::UserDataLimit):
                PAL_ASSERT(pMetadata->hasEntry.userDataLimit == 0);
                result = pReader->UnpackNext(&pMetadata->userDataLimit);
                pMetadata->hasEntry.userDataLimit = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::SpillThreshold):
                PAL_ASSERT(pMetadata->hasEntry.spillThreshold == 0);
                result = pReader->UnpackNext(&pMetadata->spillThreshold);
                pMetadata->hasEntry.spillThreshold = (result == Result::Success);;
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
                pMetadata->hasEntry.esGsLdsSize = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::NggSubgroupSize):
                PAL_ASSERT(pMetadata->hasEntry.nggSubgroupSize == 0);
                result = pReader->UnpackNext(&pMetadata->nggSubgroupSize);
                pMetadata->hasEntry.nggSubgroupSize = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::NumInterpolants):
                PAL_ASSERT(pMetadata->hasEntry.numInterpolants == 0);
                result = pReader->UnpackNext(&pMetadata->numInterpolants);
                pMetadata->hasEntry.numInterpolants = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::MeshScratchMemorySize):
                PAL_ASSERT(pMetadata->hasEntry.meshScratchMemorySize == 0);
                result = pReader->UnpackNext(&pMetadata->meshScratchMemorySize);
                pMetadata->hasEntry.meshScratchMemorySize = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::PsInputSemantic):
                PAL_ASSERT(pMetadata->hasEntry.psInputSemantic == 0);
                pReader->Next();
                result = DeserializePsInputSemanticMetadata(
                        pReader, &pMetadata->psInputSemantic[0]);
                    pMetadata->hasEntry.psInputSemantic = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::PrerasterOutputSemantic):
                PAL_ASSERT(pMetadata->hasEntry.prerasterOutputSemantic == 0);
                pReader->Next();
                result = DeserializePrerasterOutputSemanticMetadata(
                        pReader, &pMetadata->prerasterOutputSemantic[0]);
                    pMetadata->hasEntry.prerasterOutputSemantic = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::Api):
                PAL_ASSERT(pMetadata->hasEntry.api == 0);
                result = pReader->UnpackNext(&pMetadata->api);
                pMetadata->hasEntry.api = (result == Result::Success);;
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

            case HashLiteralString(PipelineMetadataKey::GsOutputsLines):
            {
                PAL_ASSERT(pMetadata->hasEntry.gsOutputsLines == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.gsOutputsLines = value;
                }

                pMetadata->hasEntry.gsOutputsLines = (result == Result::Success);
                break;
            }

            case HashLiteralString(PipelineMetadataKey::PsDummyExport):
            {
                PAL_ASSERT(pMetadata->hasEntry.psDummyExport == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.psDummyExport = value;
                }

                pMetadata->hasEntry.psDummyExport = (result == Result::Success);
                break;
            }

            case HashLiteralString(PipelineMetadataKey::PsSampleMask):
            {
                PAL_ASSERT(pMetadata->hasEntry.psSampleMask == 0);
                bool value = false;
                result = pReader->UnpackNext(&value);

                if (result == Result::Success)
                {
                    pMetadata->flags.psSampleMask = value;
                }

                pMetadata->hasEntry.psSampleMask = (result == Result::Success);
                break;
            }

            case HashLiteralString(PipelineMetadataKey::StreamoutVertexStrides):
                PAL_ASSERT(pMetadata->hasEntry.streamoutVertexStrides == 0);
                result = pReader->UnpackNext(&pMetadata->streamoutVertexStrides);
                pMetadata->hasEntry.streamoutVertexStrides = (result == Result::Success);;
                break;

            case HashLiteralString(PipelineMetadataKey::GraphicsRegisters):
                PAL_ASSERT(pMetadata->hasEntry.graphicsRegister == 0);
                pReader->Next();
                result = DeserializeGraphicsRegisterMetadata(
                        pReader, &pMetadata->graphicsRegister);
                    pMetadata->hasEntry.graphicsRegister = (result == Result::Success);
                break;

            case HashLiteralString(PipelineMetadataKey::ComputeRegisters):
                PAL_ASSERT(pMetadata->hasEntry.computeRegister == 0);
                pReader->Next();
                result = DeserializeComputeRegisterMetadata(
                        pReader, &pMetadata->computeRegister);
                    pMetadata->hasEntry.computeRegister = (result == Result::Success);
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
inline Result DeserializeCodeObjectMetadata(
    MsgPackReader*  pReader,
    CodeObjectMetadata*  pMetadata)
{
    Result result = (pReader->Type() == CWP_ITEM_MAP) ? Result::Success : Result::ErrorInvalidValue;

    for (uint32 i = pReader->Get().as.map.size; ((result == Result::Success) && (i > 0)); --i)
    {
        StringViewType key;
        result = pReader->UnpackNext(&key);

        if (result == Result::Success)
        {
            switch (HashString(key))
            {
            case HashLiteralString(CodeObjectMetadataKey::Version):
                PAL_ASSERT(pMetadata->hasEntry.version == 0);
                result = pReader->UnpackNext(&pMetadata->version);
                pMetadata->hasEntry.version = (result == Result::Success);;
                break;

            case HashLiteralString(CodeObjectMetadataKey::Pipelines):
                PAL_ASSERT(pMetadata->hasEntry.pipeline == 0);
                pReader->Next();
                result = DeserializePipelineMetadata(
                        pReader, &pMetadata->pipeline);
                    pMetadata->hasEntry.pipeline = (result == Result::Success);
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

} // PalAbi
} // Util
