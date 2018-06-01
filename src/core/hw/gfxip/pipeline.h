
/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/gpuMemory.h"
#include "palElfPackager.h"
#include "palLib.h"
#include "palMetroHash.h"
#include "palPipeline.h"
#include "palPipelineAbiProcessor.h"

namespace Pal
{

class CmdStream;
class PrefetchMgr;

// Represents information about shader operations stored obtained as shader metadata flags during processing of shader
// IL stream.
union ShaderMetadataFlags
{
    struct
    {
        uint8 writesUav : 1;   // Shader writes UAVs.
        uint8 reserved  : 7;  // Reserved for future use.
    };
    uint8 u8All;              // All the flags as a single uint.
};

// Represents per-shader metadata, obtained during processing of shader IL.
struct ShaderMetadata
{
    ShaderMetadataFlags flags[NumShaderTypes];
};

// Contains information about each API shader contained in a pipeline.
struct ShaderStageInfo
{
    // Which hardware stage the shader runs on.  Note that multiple API shaders may map to the same hardware stage
    // on some GPU's.
    Util::Abi::HardwareStage  stageId;

    size_t  codeLength;         // Length of the shader's code instructions, in bytes.
    size_t  disassemblyLength;  // Length of the shader's disassembly data, in bytes.
};

// Contains stage information calculated at pipeline bind time.
struct DynamicStageInfo
{
    uint32 wavesPerSh;
    uint32 cuEnableMask;
};

// Identifies what type of pipeline is described by a serialized pipeline ELF.
enum PipelineType : uint32
{
    PipelineTypeUnknown  = 0,
    PipelineTypeCompute  = 1,
    PipelineTypeGraphics = 2,
};

// Contains performance data information for a specific hardware stage.
struct PerfDataInfo
{
    uint32 regOffset;
    size_t cpuOffset;
    uint32 gpuVirtAddr; // Low 32 bits of the gpu virtual address.
    size_t sizeInBytes;
};

// Shorthand for a pipeline ABI processor based on the Platform allocator.
typedef Util::Abi::PipelineAbiProcessor<Platform>  AbiProcessor;

// =====================================================================================================================
// Monolithic object containing all shaders and a large amount of "shader adjacent" state.  Separate concrete
// implementations will support compute or graphics pipelines.
class Pipeline : public IPipeline
{
public:
    virtual ~Pipeline();

    void DestroyInternal();
    virtual void Destroy() override { this->~Pipeline(); }

    virtual const PipelineInfo& GetInfo() const override { return m_info; }

    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) override;

    virtual Result GetShaderCode(
        ShaderType shaderType,
        size_t*    pSize,
        void*      pBuffer) const override;

    virtual Result GetPerformanceData(
        Util::Abi::HardwareStage hardwareStage,
        size_t*                  pSize,
        void*                    pBuffer) override;

    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const override
        { return m_apiHwMapping; }

protected:
    Pipeline(Device* pDevice, bool isInternal);

    bool IsInternal() const { return m_flags.isInternal != 0; }

    Result PerformRelocationsAndUploadToGpuMemory(
        const AbiProcessor& abiProcessor,
        gpusize*            pCodeGpuVirtAddr,
        gpusize*            pDataGpuVirtAddr);

    void ExtractPipelineInfo(
        const AbiProcessor& abiProcessor,
        ShaderType          firstShader,
        ShaderType          lastShader);

    // Obtains a structure describing the traits of the hardware shader stage associated with a particular API shader
    // type.  Returns nullptr if the shader type is not present for the current pipeline.
    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const = 0;

    Result GetShaderStatsForStage(
        const ShaderStageInfo& stageInfo,
        const ShaderStageInfo* pStageInfoCopy,
        ShaderStats*           pStats) const;

    void DumpPipelineElf(
        const AbiProcessor& abiProcessor,
        const char*         pPrefix) const;

    size_t PerformanceDataSize(
        const AbiProcessor& abiProcessor) const;

    Device*const  m_pDevice;

    PipelineInfo    m_info;             // Public info structure available to the client.
    ShaderMetadata  m_shaderMetaData;   // Metadata flags for each shader type.

    BoundGpuMemory  m_gpuMem;
    gpusize         m_gpuMemSize;

    void*   m_pPipelineBinary;      // Buffer containing the pipeline binary data (Pipeline ELF ABI).
    size_t  m_pipelineBinaryLen;    // Size of the pipeline binary data, in bytes.

    PerfDataInfo m_perfDataInfo[static_cast<size_t>(Util::Abi::HardwareStage::Count)];
    Util::Abi::ApiHwShaderMapping m_apiHwMapping;

private:
    union
    {
        struct
        {
            uint32  isInternal       :  1;  // True if this Pipeline object was created internally by PAL.
            uint32  reserved         : 31;
        };
        uint32  value;  // Flags packed as a uint32.
    } m_flags;

    PAL_DISALLOW_DEFAULT_CTOR(Pipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

} // Pal
