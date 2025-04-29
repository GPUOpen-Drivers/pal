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

#include "core/hw/gfxip/archivePipeline.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "palPipelineArFile.h"
#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
ArchivePipeline::~ArchivePipeline()
{
    // Release loaded ELFs held by this pipeline.
    for (uint32 idx = 0; idx != m_loadedElfs.NumElements(); ++idx)
    {
        if (m_loadedElfs[idx] != nullptr)
        {
            m_pLoader->ReleaseLoadedElf(m_loadedElfs[idx]);
            m_loadedElfs[idx] = nullptr;
        }
    }
}

// =====================================================================================================================
// Initialize ArchivePipeline object
Result ArchivePipeline::Init(
    const ComputePipelineCreateInfo& createInfo)
{
    // Parse the archive.
    Abi::PipelineArFileReader reader(
        { static_cast<const char*>(createInfo.pPipelineBinary), createInfo.pipelineBinarySize });
    Result result = Result::Success;

    // Get a vector of the ELF members of the archive.
    Vector<Abi::PipelineArFileReader::Iterator, 8, Platform> members(m_pDevice->GetPlatform());
    for (auto member = reader.Begin(); (result == Result::Success) && (member.IsEnd() == false); member.Next())
    {
        if (member.IsMalformed())
        {
            result = Result::ErrorBadShaderCode;
            break;
        }
        else
        {
            result = members.PushBack(member);
        }
    }
    if (result == Result::Success)
    {
        result = m_loadedElfs.Resize(members.NumElements());
    }

    {
        // Load ELFs in reverse the order of members; for a ray-tracing pipeline, we need to load the lead ELF
        // last as it has relocs to other ELFs.
        for (uint32 idx = members.NumElements(); (result == Result::Success) && (idx != 0); )
        {
            const auto& member = members[--idx];
            result = LoadOneElf(createInfo,
                                member.GetData(),
                                member.GetElfHash(),
                                idx,
                                &m_loadedElfs[idx]);
        }
    }

    if (m_libraries.IsEmpty() == false)
    {
        for (IPipeline* pPipeline : m_pipelines)
        {
            static_cast<Pipeline*>(pPipeline)->MergePagingAndUploadFences(GetLibraries());
        }
    }

    if (result == Result::Success)
    {
        // Get the PipelineInfo from the first pipeline, if any.
        if (m_pipelines.IsEmpty() == false)
        {
            m_info = m_pipelines.Back()->GetInfo();
            // Propagate usesCps and cpsGlobal from constituent pipelines.
            for (IPipeline* pPipeline : m_pipelines)
            {
                m_info.flags.usesCps   |= pPipeline->GetInfo().flags.usesCps;
                m_info.flags.cpsGlobal |= pPipeline->GetInfo().flags.cpsGlobal;
            }
        }

        // Get info from the lead ELF.
        if (m_loadedElfs.IsEmpty() == false)
        {
        }
    }

    return result;
}

// =====================================================================================================================
// Load one ELF in the ArchivePipeline.
Result ArchivePipeline::LoadOneElf(
    const ComputePipelineCreateInfo& createInfo,  // Archive pipeline create info
    Span<const char>                 contents,    // Archive member to load
    uint64                           elfName,     // Name (hash) of archive member
    uint32                           currIndex,   // Current Index in the Elf Archive
    LoadedElf**                      ppLoadedElf) // (out) LoadedElf pointer to fill out
{
    Result result = Result::Success;

    {
        // Load the ELF, or find an already-loaded ELF.
        ComputePipelineCreateInfo localInfo = createInfo;
        localInfo.pPipelineBinary           = contents.Data();
        localInfo.pipelineBinarySize        = contents.NumElements();
        result                              = m_pLoader->GetElf(elfName, localInfo, m_loadedElfs, ppLoadedElf);
        if (result == Result::Success)
        {
            // If it is a pipeline, add it to the pipelines list.
            IPipeline* pPipeline = (*ppLoadedElf)->GetPipeline();
            if (pPipeline != nullptr)
            {
                result = m_pipelines.PushBack(pPipeline);
                CompilerStackSizes sizes = {};
                pPipeline->GetStackSizes(&sizes);
                m_cpsStackSizes.backendSize = Max(m_cpsStackSizes.backendSize, sizes.backendSize);
                m_cpsStackSizes.frontendSize = Max(m_cpsStackSizes.frontendSize, sizes.frontendSize);
            }
            // If it is a library, add it to the libraries list.
            else if ((*ppLoadedElf)->GetShaderLibrary())
            {
                result = m_libraries.PushBack((*ppLoadedElf)->GetShaderLibrary());

                // Get info from the lead ELF.
                if (currIndex == 0)
                {
                }
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Returns a list of GPU memory allocations used by this pipeline.
Result ArchivePipeline::QueryAllocationInfo(
    size_t*                   pNumEntries,
    GpuMemSubAllocInfo* const pAllocInfoList
    ) const
{
    Result result = Result::Success;
    size_t numEntries = 0;
    for (const IPipeline* pPipeline : m_pipelines)
    {
        size_t thisNumEntries = *pNumEntries - numEntries;
        result = pPipeline->QueryAllocationInfo(&thisNumEntries,
                                                (pAllocInfoList == nullptr) ? nullptr : pAllocInfoList + numEntries);
        if (result != Result::Success)
        {
            break;
        }
        numEntries += thisNumEntries;
    }
    *pNumEntries = numEntries;
    return result;
}

} // Pal
