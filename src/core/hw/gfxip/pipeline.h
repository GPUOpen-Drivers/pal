
/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/cmdBuffer.h"
#include "core/device.h"
#include "core/gpuMemory.h"
#include "core/dmaUploadRing.h"
#include "core/hw/gfxip/codeObjectUploader.h"
#include "palLib.h"
#include "palMetroHash.h"
#include "palPipeline.h"
#include "palPipelineAbiReader.h"
#include "palSparseVectorImpl.h"
#include "palStringView.h"
#include "palVectorImpl.h"

namespace Pal
{

class CmdBuffer;
class CmdStream;
class CodeObjectUploader;
class GraphicsShaderLibrary;

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

constexpr uint32 InvalidUserDataInternalTable = UINT32_MAX;

// Shorthand for a pipeline ABI reader.
using AbiReader = Util::Abi::PipelineAbiReader;

// Converts Pal shader type to Abi Shader type.
static Util::Abi::ApiShaderType PalShaderTypeToAbiShaderType(ShaderType stage)
{
    constexpr Util::Abi::ApiShaderType PalToAbiShaderType[] =
    {
        Util::Abi::ApiShaderType::Cs, // ShaderType::Cs
        Util::Abi::ApiShaderType::Task,
        Util::Abi::ApiShaderType::Vs, // ShaderType::Vs
        Util::Abi::ApiShaderType::Hs, // ShaderType::Hs
        Util::Abi::ApiShaderType::Ds, // ShaderType::Ds
        Util::Abi::ApiShaderType::Gs, // ShaderType::Gs
        Util::Abi::ApiShaderType::Mesh,
        Util::Abi::ApiShaderType::Ps, // ShaderType::Ps
    };
    static_assert(Util::ArrayLen(PalToAbiShaderType) == NumShaderTypes,
        "PalToAbiShaderType[] array is incorrectly sized!");
    return PalToAbiShaderType[static_cast<uint32>(stage)];
}

constexpr uint32 MaxGfxShaderLibraryCount = 3;

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
        GpuMemSubAllocInfo* const  pAllocInfoList) const override;

    virtual Result GetShaderCode(
        ShaderType shaderType,
        size_t*    pSize,
        void*      pBuffer) const override;

    virtual Result GetCodeObject(
        uint32*  pSize,
        void*    pBuffer) const override;

    virtual const void* GetCodeObjectWithShaderType(
        ShaderType shaderType,
        size_t*    pSize) const override;

    virtual Result GetPerformanceData(
        Util::Abi::HardwareStage hardwareStage,
        size_t*                  pSize,
        void*                    pBuffer) override;

    virtual Result LinkWithLibraries(
        const IShaderLibrary*const* ppLibraryList,
        uint32                      libraryCount) override;

    virtual void SetStackSizeInBytes(uint32 stackSizeInBytes) override;
    virtual Result GetStackSizes(CompilerStackSizes* pSizes) const override;

    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const override
        { return m_apiHwMapping; }

    // Unsupported in general, only compute currently has support.
    virtual const Util::HsaAbi::KernelArgument* GetKernelArgument(uint32 index) const override { return nullptr; }

    // Get the array of underlying pipelines that this pipeline contains. For a normal non-multi-pipeline,
    // this returns a single-entry array pointing to the same IPipeline.
    virtual Util::Span<const IPipeline* const> GetPipelines() const override { return m_pSelf; }

    UploadFenceToken GetUploadFenceToken() const { return m_uploadFenceToken; }
    uint64 GetPagingFenceVal() const { return m_pagingFenceVal; }

    bool IsTaskShaderEnabled() const { return (m_flags.taskShaderEnabled != 0); }

    bool IsInternal() const { return m_flags.isInternal != 0; }

    Util::Span<const void> GetPipelineBinary() const { return m_pipelineBinary; }

    static bool DispatchInterleaveSizeIsValid(
        DispatchInterleaveSize   interleave,
        const GpuChipProperties& chipProps);

    void MergePagingAndUploadFences(
        const Util::Span<const IShaderLibrary* const> libraries);

protected:
    Pipeline(Device* pDevice, bool isInternal);

    Result PerformRelocationsAndUploadToGpuMemory(
        const gpusize       performanceDataOffset,
        const GpuHeap&      clientPreferredHeap,
        CodeObjectUploader* pUploader);

    Result PerformRelocationsAndUploadToGpuMemory(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        const GpuHeap&                          clientPreferredHeap,
        CodeObjectUploader*                     pUploader);

    void ExtractPipelineInfo(
        const Util::PalAbi::CodeObjectMetadata& metadata,
        ShaderType                              firstShader,
        ShaderType                              lastShader);

    // Obtains a structure describing the traits of the hardware shader stage associated with a particular API shader
    // type.  Returns nullptr if the shader type is not present for the current pipeline.
    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const = 0;

    Result GetShaderStatsForStage(
        ShaderType             shaderType,
        const ShaderStageInfo& stageInfo,
        const ShaderStageInfo* pStageInfoCopy,
        ShaderStats*           pStats) const;

    void DumpPipelineElf(
        Util::StringView<char> prefix,
        Util::StringView<char> name) const;

    size_t PerformanceDataSize(
        const Util::PalAbi::CodeObjectMetadata& metadata) const;

    void SetTaskShaderEnabled() { m_flags.taskShaderEnabled = 1; }

    Device*const  m_pDevice;

    PipelineInfo    m_info;             // Public info structure available to the client.
    ShaderMetadata  m_shaderMetaData;   // Metadata flags for each shader type.

    BoundGpuMemory  m_gpuMem;
    gpusize         m_gpuMemSize;
    gpusize         m_gpuMemOffset;

    // Span containing the pipeline binary data in bytes.
    // Binary blob is described by the PAL pipeline ABI, or the HSA pipeline ABI, etc. in ELF/ar file format
    Util::Span<void> m_pipelineBinary;

    PerfDataInfo m_perfDataInfo[static_cast<size_t>(Util::Abi::HardwareStage::Count)];
    Util::Abi::ApiHwShaderMapping m_apiHwMapping;

    UploadFenceToken  m_uploadFenceToken;
    uint64            m_pagingFenceVal;

private:
    union
    {
        struct
        {
            uint32  isInternal             :  1;  // True if this Pipeline object was created internally by PAL.
            uint32  taskShaderEnabled      :  1;
            uint32  reserved               : 30;
        };
        uint32  value;  // Flags packed as a uint32.
    } m_flags;

    BoundGpuMemory   m_perfDataMem;
    gpusize          m_perfDataGpuMemSize;
    const IPipeline* m_pSelf;

    PAL_DISALLOW_DEFAULT_CTOR(Pipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);
};

} // Pal
