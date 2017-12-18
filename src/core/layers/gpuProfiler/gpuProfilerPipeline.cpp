/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/layers/gpuProfiler/gpuProfilerDevice.h"
#include "core/layers/gpuProfiler/gpuProfilerPipeline.h"
#include "core/layers/gpuProfiler/gpuProfilerPlatform.h"
#include "core/layers/gpuProfiler/gpuProfilerShader.h"
#include "core/layers/gpuProfiler/shaderPerfDataInfo.h"
#include "palAutoBuffer.h"
#include "palFile.h"
#include "palPipelineAbiProcessorImpl.h"

using namespace Util;
using namespace Util::Abi;

namespace Pal
{
namespace GpuProfiler
{

static_assert(((static_cast<uint32>(ApiShaderType::Cs)    == static_cast<uint32>(ShaderType::Compute))  &&
               (static_cast<uint32>(ApiShaderType::Vs)    == static_cast<uint32>(ShaderType::Vertex))   &&
               (static_cast<uint32>(ApiShaderType::Hs)    == static_cast<uint32>(ShaderType::Hull))     &&
               (static_cast<uint32>(ApiShaderType::Ds)    == static_cast<uint32>(ShaderType::Domain))   &&
               (static_cast<uint32>(ApiShaderType::Gs)    == static_cast<uint32>(ShaderType::Geometry)) &&
               (static_cast<uint32>(ApiShaderType::Ps)    == static_cast<uint32>(ShaderType::Pixel))    &&
               (static_cast<uint32>(ApiShaderType::Count) == NumShaderTypes)),
             "Util::Abi::ApiShaderType to Pal::ShaderType mapping does not match!");

// Pal::ShaderType to string conversion table.
const char* ApiShaderTypeStrings[] =
{
    "CS",
    "VS",
    "HS",
    "DS",
    "GS",
    "PS",
};

static_assert((sizeof(ApiShaderTypeStrings) / sizeof(ApiShaderTypeStrings[0])) ==
              static_cast<uint32>(ApiShaderType::Count),
              "ApiShaderTypeStrings is not the same size as Pal::ShaderType enum!");

// HardwareStage to string conversion table.
const char* HardwareStageStrings[] =
{
    "LS",
    "HS",
    "ES",
    "GS",
    "VS",
    "PS",
    "CS",
    "INVALID",
};

static_assert((sizeof(HardwareStageStrings) / sizeof(HardwareStageStrings[0])) ==
              static_cast<uint32>(HardwareStage::Count) + 1,
              "HardwareStageStrings is not the same size as HardwareStage enum!");

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
    const char*const pLogDir = static_cast<const Platform*>(m_pDevice->GetPlatform())->LogDirName();
    char fileName[512] = {};

    PAL_ASSERT(ShaderHashIsNonzero(dumpInfo.hash));

    Util::Snprintf(&fileName[0], sizeof(fileName), "%s/0x%016llX%016llX_%s.spd",
                   pLogDir,
                   dumpInfo.hash.upper,
                   dumpInfo.hash.lower,
                   ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)]);

    const bool isDuplicate = Util::File::Exists(&fileName[0]);
    if (isDuplicate)
    {
        // The assembled filename already exists, so perform a binary search to find an unused filename formed
        // by the original filename with a monotonically-increasing numeric suffix.  This idea was adapted from
        // a snippet found online here:
        // https://stackoverflow.com/questions/1078003/c-how-would-you-make-a-unique-filename-by-adding-a-number

        // The index into the string where ".txt" begins.
        const size_t endOfName = (strlen(fileName) - 4);
        const size_t suffixLen = (sizeof(fileName) - endOfName);
        char*const   pSuffixPos = &fileName[endOfName];

        uint32 suffixMin = 0;
        uint32 suffixMax = 1;
        Util::Snprintf(pSuffixPos, suffixLen, "-[%d].txt", suffixMax);

        // Keep doubling the maximum numeric suffix until we determine a range of numbers which are unused.
        while (Util::File::Exists(&fileName[0]))
        {
            suffixMin = suffixMax;
            suffixMax *= 2;

            Util::Snprintf(pSuffixPos, suffixLen, "-[%d].txt", suffixMax);
        }

        // If that unused range contains more than one number in it, do a binary search to find the lowest one.
        if (suffixMax != (suffixMin + 1))
        {
            while (suffixMax != (suffixMin + 1))
            {
                const uint32 suffixPivot = ((suffixMax + suffixMin) / 2);
                Util::Snprintf(pSuffixPos, suffixLen, "-[%d].txt", suffixPivot);

                if (Util::File::Exists(&fileName[0]))
                {
                    suffixMin = suffixPivot;
                }
                else
                {
                    suffixMax = suffixPivot;
                }
            }

            // At this point, suffixMax is the number which represents the next available filename/suffix
            // combination.
            Util::Snprintf(pSuffixPos, suffixLen, "-[%d].txt", suffixMax);
        }
    }

    if (isDuplicate == false)
    {
        // We've computed a unique file name, so open the text file for write access.
        const Result result = dumpInfo.pFile->Open(&fileName[0], Util::FileAccessBinary | Util::FileAccessWrite);
        PAL_ASSERT(result == Result::Success);

        if (result == Result::Success)
        {
            // Write the header out now. This will be overwritten later with more accurate data, but we want to
            // account for it now.
            ShaderPerfData::PerformanceDataHeader header = {};
            header.version               = ShaderPerfData::PerformanceDataHeader::HeaderVersion;
            Util::Strncpy(&header.apiShaderType[0],
                            ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)],
                            sizeof(header.apiShaderType));
            header.shaderHash.lower      = dumpInfo.hash.lower;
            header.shaderHash.upper      = dumpInfo.hash.upper;
            header.pipelineHash          = dumpInfo.pipelineHash;
            header.compilerHash          = dumpInfo.compilerHash;
            header.payloadSize           = 0;
            header.numShaderChunks       = 0;
            dumpInfo.pFile->Write(&header, sizeof(header));
        }
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
                  HardwareStageStrings[static_cast<uint32>(dumpInfo.hwStage)],
                  sizeof(header.hwShaderType));
    header.payloadSize  = perfDataSize;

    dumpInfo.pFile->Write(&header, sizeof(header));
    dumpInfo.pFile->Write(pPerfData, perfDataSize);

    return sizeof(header) + perfDataSize;
}

// =====================================================================================================================
void Pipeline::Destroy()
{
    const auto& info = GetInfo();

    // Pipelines can only be destroyed if they are not being used by the GPU, so it is safe to perform the performance
    // data retrieval now.
    for (uint32 i = 0; i < static_cast<uint32>(ApiShaderType::Count); i++)
    {
        if ((ShaderHashIsNonzero(info.shader[i].hash)) && (m_apiHwMapping.apiShaders[i] != 0))
        {
            const ApiShaderType type = static_cast<ApiShaderType>(i);
            Util::File file;

            ShaderDumpInfo dumpInfo = {};
            dumpInfo.pipelineHash   = info.pipelineHash;
            dumpInfo.compilerHash   = info.compilerHash;
            dumpInfo.type           = type;
            dumpInfo.hash           = info.shader[i].hash;
            dumpInfo.pFile          = &file;

            uint32     numShaders  = 0;
            size_t     payloadSize = 0;

            MutexAuto lock(m_pPlatform->PipelinePerfDataLock());
            const bool isOpened = OpenUniqueDumpFile(dumpInfo);

            if (isOpened)
            {
                uint32 mapping = m_apiHwMapping.apiShaders[i];

                uint32 bitIndex = 0;
                while (Util::BitMaskScanForward(&bitIndex, mapping))
                {
                    // We need to mask off the bit we just found to prevent an infinite loop.
                    mapping &= ~(1 << bitIndex);

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
                            dumpInfo.pipelineHash = info.pipelineHash;
                            dumpInfo.compilerHash = info.compilerHash;

                            payloadSize += DumpShaderPerfData(dumpInfo, &data[0], size);
                            numShaders++;
                        }
                    }
                }
            }

            // Before we close the file, we should update the file header.
            ShaderPerfData::PerformanceDataHeader header = {};
            header.version               = ShaderPerfData::PerformanceDataHeader::HeaderVersion;
            Util::Strncpy(&header.apiShaderType[0],
                            ApiShaderTypeStrings[static_cast<uint32>(dumpInfo.type)],
                            sizeof(header.apiShaderType));
            header.shaderHash.lower      = dumpInfo.hash.lower;
            header.shaderHash.upper      = dumpInfo.hash.upper;
            header.pipelineHash          = dumpInfo.pipelineHash;
            header.compilerHash          = dumpInfo.compilerHash;
            header.payloadSize           = payloadSize;
            header.numShaderChunks       = numShaders;

            file.Rewind();
            file.Write(&header, sizeof(header));
            file.Close();
        }
    }

    PipelineDecorator::Destroy();
}

// =====================================================================================================================
Result Pipeline::InitGfx(
    const GraphicsPipelineCreateInfo& createInfo)
{
    Result result = Result::ErrorInvalidPointer;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 305
    if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize > 0))
    {
        PipelineAbiProcessor<PlatformDecorator> abiProcessor(m_pDevice->GetPlatform());
        result = abiProcessor.LoadFromBuffer(createInfo.pPipelineBinary, createInfo.pipelineBinarySize);

        if (result == Result::Success)
        {
            static_assert(((static_cast<uint32>(HardwareStage::Cs) + 1) ==
                            static_cast<uint32>(HardwareStage::Count)),
                          "HardwareStage::Cs is not located at the end of the HardwareStage enum!");

            // We need to check if any graphics stage contains performance data.
            for (uint32 i = 0; i < static_cast<uint32>(HardwareStage::Cs); i++)
            {
                const PipelineMetadataType type =
                    GetMetadataForStage(PipelineMetadataType::ShaderPerformanceDataBufferSize,
                                        static_cast<HardwareStage>(i));

                if (abiProcessor.HasPipelineMetadataEntry(type))
                {
                    // If the ELF contains any of the performance data buffer size entries, then one of the stages
                    // contains performance data.
                    m_hasPerformanceData = true;
                    break;
                }
            }

            if (abiProcessor.HasPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingLo) &&
                abiProcessor.HasPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingHi))
            {
                m_apiHwMapping.u32Lo =
                    abiProcessor.GetPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingLo);
                m_apiHwMapping.u32Hi =
                    abiProcessor.GetPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingHi);
            }
            else
            {
                result = Result::Unsupported;
            }
        }
    }
    else
#endif
    {
        if ((createInfo.vs.pShader != nullptr) &&
            (static_cast<const Shader*>(createInfo.vs.pShader)->HasPerformanceData()))
        {
            m_hasPerformanceData = true;
        }

        if ((m_hasPerformanceData == false)    &&
            (createInfo.hs.pShader != nullptr) &&
            (static_cast<const Shader*>(createInfo.hs.pShader)->HasPerformanceData()))
        {
            m_hasPerformanceData = true;
        }

        if ((m_hasPerformanceData == false) &&
            (createInfo.ds.pShader != nullptr) &&
            (static_cast<const Shader*>(createInfo.ds.pShader)->HasPerformanceData()))
        {
            m_hasPerformanceData = true;
        }

        if ((m_hasPerformanceData == false) &&
            (createInfo.gs.pShader != nullptr) &&
            (static_cast<const Shader*>(createInfo.gs.pShader)->HasPerformanceData()))
        {
            m_hasPerformanceData = true;
        }

        if ((m_hasPerformanceData == false) &&
            (createInfo.ps.pShader != nullptr) &&
            (static_cast<const Shader*>(createInfo.ps.pShader)->HasPerformanceData()))
        {
            m_hasPerformanceData = true;
        }

        m_apiHwMapping = m_pNextLayer->ApiHwShaderMapping();
        result         = Result::Success;
    }

    return result;
}

// =====================================================================================================================
Result Pipeline::InitCompute(
    const ComputePipelineCreateInfo& createInfo)
{
    Result result = Result::ErrorInvalidPointer;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 305
    if ((createInfo.pPipelineBinary != nullptr) && (createInfo.pipelineBinarySize > 0))
    {
        PipelineAbiProcessor<PlatformDecorator> abiProcessor(m_pDevice->GetPlatform());
        result = abiProcessor.LoadFromBuffer(createInfo.pPipelineBinary, createInfo.pipelineBinarySize);

        if (result == Result::Success)
        {
            m_hasPerformanceData =
                abiProcessor.HasPipelineMetadataEntry(PipelineMetadataType::CsPerformanceDataBufferSize);

            if (abiProcessor.HasPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingLo) &&
                abiProcessor.HasPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingHi))
            {
                m_apiHwMapping.u32Lo =
                    abiProcessor.GetPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingLo);
                m_apiHwMapping.u32Hi =
                    abiProcessor.GetPipelineMetadataEntry(PipelineMetadataType::ApiHwShaderMappingHi);
            }
            else
            {
                result = Result::Unsupported;
            }
        }
    }
    else
#endif
    {
        m_hasPerformanceData = static_cast<const Shader*>(createInfo.cs.pShader)->HasPerformanceData();
        m_apiHwMapping       = m_pNextLayer->ApiHwShaderMapping();
        result               = Result::Success;
    }

    return result;
}

} // InterfaceLogger
} // Pal
