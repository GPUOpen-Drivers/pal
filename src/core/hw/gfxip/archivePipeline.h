/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/pipeline.h"
#include "core/hw/gfxip/pipelineLoader.h"

namespace Pal
{

// =====================================================================================================================
// ArchivePipeline represents a pipeline containing multiple ELFs in an archive.
class ArchivePipeline : public Pipeline
{
public:
    ArchivePipeline(Device* pDevice, bool isInternal)
        : Pipeline(pDevice, isInternal),
          m_pLoader(pDevice->GetGfxDevice()->GetPipelineLoader()),
          m_loadedElfs(pDevice->GetPlatform()),
          m_pipelines(pDevice->GetPlatform()),
          m_libraries(pDevice->GetPlatform()),
          m_info(),
          m_cpsStackSizes({})
        {}

    virtual ~ArchivePipeline();

    // Initialize the object
    Result Init(const ComputePipelineCreateInfo& createInfo);

    // Destroy the object
    virtual void Destroy() override { this->~ArchivePipeline(); }

    // Returns PAL-computed properties of this pipeline and its corresponding shaders.
    virtual const PipelineInfo& GetInfo() const override { return m_info; }

    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const override
    {
        return m_pipelines.IsEmpty() ? nullptr
                                     : static_cast<ArchivePipeline*>(LeadPipeline())->GetShaderStageInfo(shaderType);
    }

    // Returns a list of GPU memory allocations used by this pipeline.
    virtual Result QueryAllocationInfo(size_t*                   pNumEntries,
                                       GpuMemSubAllocInfo* const pAllocInfoList) const override;

    // Obtains the binary code object for this pipeline.
    virtual Result GetCodeObject(uint32* pSize, void* pBuffer) const override
        { return m_pipelines.IsEmpty() ? Result::ErrorUnavailable : LeadPipeline()->GetCodeObject(pSize, pBuffer); }

    // Obtains the shader pre and post compilation stats/params for the specified shader stage.
    virtual Result GetShaderStats(ShaderType   shaderType,
                                  ShaderStats* pShaderStats,
                                  bool         getDisassemblySize) const override
    {
        return m_pipelines.IsEmpty() ? Result::ErrorUnavailable
                                     : LeadPipeline()->GetShaderStats(shaderType, pShaderStats, getDisassemblySize);
    }

    // Obtains the compiled shader ISA code for the shader stage specified.
    virtual Result GetShaderCode(ShaderType shaderType,
                                 size_t*    pSize,
                                 void*      pBuffer) const override
    {
        return m_pipelines.IsEmpty() ? Result::ErrorUnavailable
                                     : LeadPipeline()->GetShaderCode(shaderType, pSize, pBuffer);
    }

    // Obtains the generated performance data for the shader stage specified.
    virtual Result GetPerformanceData(Util::Abi::HardwareStage hardwareStage,
                                      size_t*                  pSize,
                                      void*                    pBuffer) override
    {
        return m_pipelines.IsEmpty() ? Result::ErrorUnavailable
                                     : LeadPipeline()->GetPerformanceData(hardwareStage, pSize, pBuffer);
    }

    // Notifies PAL that this pipeline may make indirect function calls to any function contained within any of the
    // specified @ref IShaderLibrary objects.
    virtual Result LinkWithLibraries(const IShaderLibrary*const* ppLibraryList,
                                     uint32                      libraryCount) override
    {
        return m_pipelines.IsEmpty() ? Result::ErrorUnavailable
                                     : LeadPipeline()->LinkWithLibraries(ppLibraryList, libraryCount);
    }

    // Sets the stack size for indirect function calls made by this pipeline.
    virtual void SetStackSizeInBytes(uint32 stackSizeInBytes) override
    {
        if (m_pipelines.IsEmpty() == false)
        {
            LeadPipeline()->SetStackSizeInBytes(stackSizeInBytes);
        }
    }

    // Retrieve the stack sizes managed by compiler, including the frontend stack and the backend stack.
    virtual Result GetStackSizes(CompilerStackSizes* pSizes) const override
    {
        *pSizes = m_cpsStackSizes;
        return Result::Success;
    }

    // Returns the API shader type to hardware stage mapping for the pipeline.
    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const override
        { return m_pipelines.IsEmpty() ? Util::Abi::ApiHwShaderMapping{} : LeadPipeline()->ApiHwShaderMapping(); }

    // Given the zero-based position of a kernel argument, return a pointer to that argument's metadata.
    virtual const Util::HsaAbi::KernelArgument* GetKernelArgument(uint32 index) const override
        { return m_pipelines.IsEmpty() ? nullptr : LeadPipeline()->GetKernelArgument(index); }

    // Get the array of underlying pipelines that this pipeline contains. For a multi-pipeline compiled in
    // dynamic launch mode, this returns an empty array.
    virtual Util::Span<const IPipeline* const> GetPipelines() const override
        { return { m_pipelines.Data(), m_pipelines.NumElements() }; }

    // Get the array of underlying shader libraries that this pipeline contains.
    virtual Util::Span<const IShaderLibrary* const> GetLibraries() const override
        { return { m_libraries.Data(), m_libraries.NumElements() }; }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(ArchivePipeline);

    Pipeline* LeadPipeline() const { return static_cast<Pipeline*>(m_pipelines.Back()); }

    PipelineLoader*                            m_pLoader;
    Util::Vector<LoadedElf*, 8, Platform>      m_loadedElfs;
    Util::Vector<IPipeline*, 8, Platform>      m_pipelines;
    Util::Vector<IShaderLibrary*, 8, Platform> m_libraries;
    PipelineInfo                               m_info;
    CompilerStackSizes                         m_cpsStackSizes;
};

} // Pal
