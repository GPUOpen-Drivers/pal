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

#pragma once

#include "core/hw/gfxip/shaderLibraryBase.h"
#include "palVector.h"

namespace Pal
{

class GfxDevice;
class LoadedElf;
class PipelineLoader;
class Platform;

// =====================================================================================================================
// ArchiveLibrary is an IShaderLibrary implementation for an archive of ELFs, with GPU memory deduplication
class ArchiveLibrary : public ShaderLibraryBase
{
public:
    ArchiveLibrary(GfxDevice* pGfxDevice, const ShaderLibraryCreateInfo& createInfo);

    // Initialize the object.
    Result Init(const ShaderLibraryCreateInfo& createInfo);

    // Destroy the object
    virtual void Destroy() override { this->~ArchiveLibrary(); }

    // Returns properties of this library and its corresponding shader functions.
    virtual const LibraryInfo& GetInfo() const override;

    // Returns a list of GPU memory allocations used by this library.
    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const override
            { return Result::ErrorUnavailable; }

    // Obtains the binary code object for this library.
    virtual Result GetCodeObject(
        uint32*  pSize,
        void*    pBuffer) const override
            { return Result::ErrorUnavailable; }

    // Obtains the compiled shader ISA code for the shader function specified.
    virtual Result GetShaderFunctionCode(
        Util::StringView<char> shaderExportName,
        size_t*                pSize,
        void*                  pBuffer) const override
            { return Result::ErrorUnavailable; }

    // Obtains the shader pre and post compilation stats/params for the specified shader.
    virtual Result GetShaderFunctionStats(
        Util::StringView<char> shaderExportName,
        ShaderLibStats*        pShaderStats) const override
            { return Result::ErrorUnavailable; }

    // Returns the function list owned by this shader library
    virtual const Util::Span<const ShaderLibraryFunctionInfo> GetShaderLibFunctionInfos() const override
            { return {}; }

    // Returns array of underlying singleton ShaderLibraries. If this is already a singleton ShaderLibrary,
    // just returns a pointer to itself.
    virtual ShaderLibrarySpan GetShaderLibraries() const override { return m_shaderLibraries; }

private:
    ArchiveLibrary(const ArchiveLibrary&) = delete;
    ArchiveLibrary& operator=(const ArchiveLibrary&) = delete;

    ~ArchiveLibrary();

    GfxDevice* const                                m_pGfxDevice;
    PipelineLoader* const                           m_pLoader;
    Util::Vector<const ShaderLibrary*, 8, Platform> m_shaderLibraries;
    Util::Vector<LoadedElf*, 8, Platform>           m_loadedElfs;
};

} // Pal
