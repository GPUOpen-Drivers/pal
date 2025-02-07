/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPipeline.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/shaderPerfDataInfo.h"
#include "palAutoBuffer.h"
#include "palFile.h"
#include "palPipelineAbiReader.h"
#include "palIterator.h"

using namespace Util;
using namespace Util::Abi;

namespace Pal
{
namespace GpuProfiler
{

// Pal::ShaderType to string conversion table.
constexpr const char* ApiShaderTypeStrings[] =
{
    "CS",
    "TASK",
    "VS",
    "HS",
    "DS",
    "GS",
    "MESH",
    "PS",
};

static_assert(ArrayLen(ApiShaderTypeStrings) == static_cast<uint32>(ApiShaderType::Count),
              "ApiShaderTypeStrings is not the same size as Abi::ApiShaderType enum!");

// =====================================================================================================================
Pipeline::Pipeline(
    IPipeline*    pNextPipeline,
    const Device* pDevice)
    :
    PipelineDecorator(pNextPipeline, pDevice),
    m_pDevice(pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_hasPerformanceData(false),
    m_apiHwMapping()
{
    m_apiHwMapping.u64All = 0;
}

// =====================================================================================================================
// Helper function which opens a log file for dumping the performance data of shaders.
// Returns: Whether or not the dump file was opened.
bool Pipeline::OpenUniqueDumpFile(
    const ShaderDumpInfo& dumpInfo
    ) const
{
    char fileName[512] = {};

    PAL_ASSERT(ShaderHashIsNonzero(dumpInfo.hash));

    const size_t nextPos = Util::Snprintf(&fileName[0], sizeof(fileName), "%s/0x%016llX%016llX_%s",
                                          static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirPath(),
                                          dumpInfo.hash.upper,
                                          dumpInfo.hash.lower,
                                          ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)]);

    Util::GenLogFilename(fileName, sizeof(fileName), nextPos, ".spd", true);

    // We've computed a unique file name, so open the text file for write access.
    const Result result = dumpInfo.pFile->Open(&fileName[0], Util::FileAccessBinary | Util::FileAccessWrite);
    PAL_ASSERT(result == Result::Success);

    if (result == Result::Success)
    {
        // Write the header out now. This will be overwritten later with more accurate data, but we want to
        // account for it now.
        ShaderPerfData::PerformanceDataHeader header = {};
        header.version               = ShaderPerfData::HeaderVersion;
        Util::Strncpy(&header.apiShaderType[0],
                      ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)],
                      sizeof(header.apiShaderType));
        header.shaderHash.lower      = dumpInfo.hash.lower;
        header.shaderHash.upper      = dumpInfo.hash.upper;
        header.compilerHash          = dumpInfo.compilerHash;
        header.payloadSize           = 0;
        header.numShaderChunks       = 0;
        dumpInfo.pFile->Write(&header, sizeof(header));
    }

    return dumpInfo.pFile->IsOpen();
}

// =====================================================================================================================
// Opens a unique file to dump the specified shader's performance data into.
size_t Pipeline::DumpShaderPerfData(
    const ShaderDumpInfo& dumpInfo,
    void*                 pPerfData,
    size_t                perfDataSize
    ) const
{
    ShaderPerfData::PerformanceDataShaderHeader header = {};

    header.chunkType = ShaderPerfData::ChunkType::Shader;
    Util::Strncpy(&header.hwShaderType[0],
                  Util::Abi::HardwareStageStrings[static_cast<uint32>(dumpInfo.hwStage)],
                  sizeof(header.hwShaderType));
    header.payloadSize  = perfDataSize;

    dumpInfo.pFile->Write(&header, sizeof(header));
    dumpInfo.pFile->Write(pPerfData, perfDataSize);

    return sizeof(header) + perfDataSize;
}

// =====================================================================================================================
void Pipeline::Destroy()
{

    // A new path ray-tracing "pipeline" is an archive with possibly multiple compute pipelines (or none).
    // A new path workgraphs "pipeline" is an archive with no compute pipelines.
    // If it is an archive, process each compute pipeline.
    Util::Span<const IPipeline* const> pipelines = GetPipelines();
    for (const IPipeline* pPipeline : pipelines)
    {
        DumpPipelinePerfData(pPipeline);
    }

    PipelineDecorator::Destroy();
}

// =====================================================================================================================
// Dump the perf data for a single non-archive pipeline.
void Pipeline::DumpPipelinePerfData(
    const IPipeline* pPipeline) // (in) The non-archive pipeline to dump perf data from
{
    const auto& info = pPipeline->GetInfo();

    // Pipelines can only be destroyed if they are not being used by the GPU, so it is safe to perform the performance
    // data retrieval now.
    for (uint32 i = 0; (m_hasPerformanceData && (i < static_cast<uint32>(ApiShaderType::Count))); i++)
    {
        if ((ShaderHashIsNonzero(info.shader[i].hash)) && (m_apiHwMapping.apiShaders[i] != 0))
        {
            const ApiShaderType type = static_cast<ApiShaderType>(i);
            Util::File file;

            ShaderDumpInfo dumpInfo = {};
            dumpInfo.compilerHash = info.internalPipelineHash.stable;
            dumpInfo.type         = type;
            dumpInfo.hash         = info.shader[i].hash;
            dumpInfo.pFile        = &file;

            uint32     numShaders  = 0;
            size_t     payloadSize = 0;

            MutexAuto lock(m_pPlatform->PipelinePerfDataLock());
            const bool isOpened = OpenUniqueDumpFile(dumpInfo);

            if (isOpened)
            {
                uint32 mapping = m_apiHwMapping.apiShaders[i];

                for (uint32 bitIndex : BitIter32(mapping))
                {
                    const HardwareStage hwStage = static_cast<HardwareStage>(bitIndex);

                    size_t size   = 0;
                    Result result = GetPerformanceData(hwStage, &size, nullptr);

                    if ((result == Result::Success) && (size > 0))
                    {
                        Util::AutoBuffer<char, 256, PlatformDecorator> data(size, m_pDevice->GetPlatform());

                        result = GetPerformanceData(hwStage, &size, &data[0]);

                        if (result == Result::Success)
                        {
                            dumpInfo.type         = type;
                            dumpInfo.hwStage      = hwStage;
                            dumpInfo.hash         = info.shader[i].hash;

                            payloadSize += DumpShaderPerfData(dumpInfo, &data[0], size);
                            numShaders++;
                        }
                    }
                }
            }

            // Before we close the file, we should update the file header.
            ShaderPerfData::PerformanceDataHeader header = {};
            header.version               = ShaderPerfData::HeaderVersion;
            Util::Strncpy(&header.apiShaderType[0],
                          ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)],
                          sizeof(header.apiShaderType));
            header.shaderHash.lower      = dumpInfo.hash.lower;
            header.shaderHash.upper      = dumpInfo.hash.upper;
            header.compilerHash          = dumpInfo.compilerHash;
            header.payloadSize           = payloadSize;
            header.numShaderChunks       = numShaders;

            file.Rewind();
            file.Write(&header, sizeof(header));
            file.Close();
        }
    }
}

// =====================================================================================================================
Result Pipeline::InitGfx()
{
    Result result = PipelineDecorator::Init();

    if (result == Result::Success)
    {
        static_assert(((static_cast<uint32>(HardwareStage::Cs) + 1) ==
            static_cast<uint32>(HardwareStage::Count)),
            "HardwareStage::Cs is not located at the end of the HardwareStage enum!");

        // We need to check if any graphics stage contains performance data.
        for (uint32 i = 0; i < static_cast<uint32>(HardwareStage::Cs); i++)
        {
            size_t perfDataSize = 0;
            const Result getPerfDataResult = m_pNextLayer->GetPerformanceData(HardwareStage(i), &perfDataSize, nullptr);
            if ((getPerfDataResult == Result::Success) && (perfDataSize > 0))
            {
                m_hasPerformanceData = true;
                break;
            }

        }
        m_apiHwMapping = m_pNextLayer->ApiHwShaderMapping();
    }
    return result;
}

// =====================================================================================================================
Result Pipeline::InitCompute(
    const ComputePipelineCreateInfo& createInfo)
{
    Util::Span<const IPipeline* const> pipelines;
    Result result              = PipelineDecorator::Init();
    const IPipeline* pPipeline = nullptr;
    void* pElfBuffer           = nullptr;

    // A new path ray-tracing "pipeline" is an archive with possibly multiple compute pipelines (or none).
    // A new path workgraphs "pipeline" is an archive with no compute pipelines.
    // If it is an archive, get the first compute pipeline if any, then get the ELF, and parse metadata from that.
    if (result == Result::Success)
    {
        pipelines = GetPipelines();
    }
    if (pipelines.IsEmpty() == false)
    {
        pPipeline = pipelines[0];
    }

    uint32 size = 0;
    if (pPipeline != nullptr)
    {
        result = pPipeline->GetCodeObject(&size, nullptr);
        if (result == Result::Success)
        {
            pElfBuffer = PAL_MALLOC(size, m_pDevice->GetPlatform(), Util::AllocInternal);
            result = Result::ErrorOutOfMemory;
            if (pElfBuffer != nullptr)
            {
                result = pPipeline->GetCodeObject(&size, pElfBuffer);
            }
        }
    }

    if ((result == Result::Success) && (pElfBuffer != nullptr))
    {
        PipelineAbiReader abiReader(m_pDevice->GetPlatform(), {pElfBuffer, size});
        result = abiReader.Init();

        MsgPackReader              metadataReader;
        PalAbi::CodeObjectMetadata metadata;

        if (result == Result::Success)
        {
            result = abiReader.GetMetadata(&metadataReader, &metadata);
        }

        if (result == Result::Success)
        {
            static constexpr uint32 HwStageCs = static_cast<uint32>(HardwareStage::Cs);
            m_hasPerformanceData = (metadata.pipeline.hardwareStage[HwStageCs].hasEntry.perfDataBufferSize != 0);

            result = Result::Unsupported;

            for (uint32 s = 0; s < static_cast<uint32>(ApiShaderType::Count); ++s)
            {
                if (metadata.pipeline.shader[s].hasEntry.hardwareMapping)
                {
                    m_apiHwMapping.apiShaders[s] = static_cast<uint8>(metadata.pipeline.shader[s].hardwareMapping);
                    result = Result::Success;
                }
            }
        }

        PAL_FREE(pElfBuffer, m_pDevice->GetPlatform());
    }

    // This function only exists to parse some PAL ABI metadata from the ELF. It's not its job to validate the ELF.
    // If this code thinks the ELF is invalid that's OK, we can just force off the performance data feature. The core
    // PAL code will return an error instead if the ELF is really invalid.
    if (result != Result::Success)
    {
        m_hasPerformanceData = false;
    }

    return Result::Success;
}

} // GpuProfiler
} // Pal
