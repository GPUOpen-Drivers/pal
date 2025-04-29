/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

// ArchiveLibrary is an IShaderLibrary implementation for an archive of ELFs, with GPU memory deduplication.

#include "core/hw/gfxip/archiveLibrary.h"
#include "core/hw/gfxip/pipelineLoader.h"
#include "core/hw/gfxip/shaderLibrary.h"
#include "palPipelineArFile.h"
#include "palVectorImpl.h"

using namespace Pal;
using namespace Util;

// =====================================================================================================================
// Destructor
ArchiveLibrary::~ArchiveLibrary()
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
// Constructor
ArchiveLibrary::ArchiveLibrary(
    GfxDevice*                     pGfxDevice,
    const ShaderLibraryCreateInfo& createInfo)
    :
    m_pGfxDevice(pGfxDevice),
    m_pLoader(pGfxDevice->GetPipelineLoader()),
    m_shaderLibraries(pGfxDevice->GetPlatform()),
    m_loadedElfs(pGfxDevice->GetPlatform())
{
}

// =====================================================================================================================
// Initialize the object.
Result ArchiveLibrary::Init(
    const ShaderLibraryCreateInfo& createInfo)
{
    // Parse the archive.
    Abi::PipelineArFileReader reader(
        { static_cast<const char*>(createInfo.pCodeObject), createInfo.codeObjectSize });
    Result result = Result::Success;

    // Load (or find already loaded) each ELF in turn.
    ShaderLibraryCreateInfo localInfo = createInfo;
    for (auto member = reader.Begin(); (result == Result::Success) && (member.IsEnd() == false); member.Next())
    {
        if (member.IsMalformed())
        {
            result = Result::ErrorBadShaderCode;
            break;
        }

        result = m_loadedElfs.PushBack({});
        if (result != Result::Success)
        {
            break;
        }

        // The ELF name is a 64-bit hash.
        const uint64 elfName = member.GetElfHash();

        // Load the ELF, or find an already-loaded ELF.
        const Span<const char> contents = member.GetData();
        localInfo.pCodeObject           = contents.Data();
        localInfo.codeObjectSize        = contents.NumElements();

        result = m_pLoader->GetElf(elfName, localInfo, &m_loadedElfs.Back());
        if (result == Result::Success)
        {
            result = m_shaderLibraries.PushBack(static_cast<ShaderLibrary*>(m_loadedElfs.Back()->GetShaderLibrary()));
        }
    }

    return result;
}

// =====================================================================================================================
// Returns properties of this library and its corresponding shader functions.
const LibraryInfo& ArchiveLibrary::GetInfo() const
{
    PAL_ASSERT_ALWAYS_MSG("not implemented");
    static const LibraryInfo nullLibraryInfo{};
    return nullLibraryInfo;
}
