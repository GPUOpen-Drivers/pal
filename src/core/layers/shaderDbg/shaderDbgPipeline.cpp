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

#include "core/layers/shaderDbg/shaderDbgDevice.h"
#include "core/layers/shaderDbg/shaderDbgPipeline.h"
#include "core/layers/shaderDbg/shaderDbgPlatform.h"
#include "core/g_palPlatformSettings.h"
#include "palAutoBuffer.h"
#include "palFile.h"
#include "palPipelineAbiProcessorImpl.h"
#include "palSysUtil.h"
#include "shaderDbgTypes.h"
#include <cinttypes>

using namespace Util;
using namespace Util::Abi;

namespace Pal
{
namespace ShaderDbg
{

// HardwareStage to string conversion table.
const char* HardwareStageStrings[] =
{
    "HS",
    "GS",
    "VS",
    "PS",
    "CS",
};

static_assert(ArrayLen(HardwareStageStrings) == Sdl_HwShaderStage_Count,
              "HardwareStageStrings is not the same size as HardwareStage enum!");

// =====================================================================================================================
Pipeline::Pipeline(
    IPipeline*    pNextPipeline,
    const Device* pDevice)
    :
    PipelineDecorator(pNextPipeline, pDevice),
    m_pDevice(pDevice),
    m_pPlatform(static_cast<Platform*>(pDevice->GetPlatform())),
    m_hwShaderDbgMask(0),
    m_apiHwMapping()
{
    m_apiHwMapping.u64All = 0;
}

// =====================================================================================================================
// Helper function which opens a log file.
// Returns: Whether or not the dump file was opened.
bool Pipeline::OpenUniqueDumpFile(
    const ShaderDumpInfo& dumpInfo
    ) const
{
    char fileName[512] = {};

    const char* pDrawString     = "DRAW";
    const char* pDispatchString = "DISPATCH";

    // This will create the log directory the first time it is called.
    Result result = m_pPlatform->CreateLogDir(m_pPlatform->PlatformSettings().shaderDbgConfig.shaderDbgDirectory);

    if (result == Result::Success)
    {
        Util::Snprintf(fileName,
                       sizeof(fileName),
                       "%s/0x%016llX_0x%016llX",
                       m_pPlatform->LogDirPath(),
                       dumpInfo.compilerHash,
                       dumpInfo.pipelineHash);
        result = Util::MkDir(&fileName[0]);
    }

    if ((result == Result::Success) || (result == Result::AlreadyExists))
    {
        const size_t endOfString = strlen(&fileName[0]);
        Util::Snprintf(&fileName[endOfString], sizeof(fileName) - endOfString, "/%s_%d_%d_%s.sdl",
                       (dumpInfo.isDraw) ? pDrawString : pDispatchString,
                       dumpInfo.submitId,
                       dumpInfo.uniqueId,
                       HardwareStageStrings[static_cast<uint32>(dumpInfo.hwStage)]);

        PAL_ASSERT(Util::File::Exists(&fileName[0]) == false);

        // Open the text file for write access.
        result = dumpInfo.pFile->Open(&fileName[0], Util::FileAccessBinary | Util::FileAccessWrite);
    }

    PAL_ASSERT(result == Result::Success);

    return ((result == Result::Success) && dumpInfo.pFile->IsOpen());
}

// =====================================================================================================================
Result Pipeline::Init(
    const void* pPipelineBinary,
    size_t      pipelineBinarySize)
{
    PAL_ASSERT((pPipelineBinary != nullptr) && (pipelineBinarySize > 0));
    PipelineAbiProcessor<Platform> abiProcessor(m_pPlatform);
    Result result = abiProcessor.LoadFromBuffer(pPipelineBinary, pipelineBinarySize);

    MsgPackReader              metadataReader;
    Abi::PalCodeObjectMetadata metadata;

    if (result == Result::Success)
    {
        abiProcessor.GetMetadata(&metadataReader, &metadata);
    }

    if (result == Result::Success)
    {
        for (uint32 s = 0; s < static_cast<uint32>(ApiShaderType::Count); ++s)
        {
            m_apiHwMapping.apiShaders[s] = static_cast<uint8>(metadata.pipeline.shader[s].hardwareMapping);
        }

        m_hwShaderDbgMask = metadata.pipeline.debugHwStages;
    }

    return result;
}

} // ShaderDbg
} // Pal
