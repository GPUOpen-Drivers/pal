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

#pragma once

#include "pal.h"
#include "palElf.h"
#include "palFunctionRef.h"
#include "palHashMap.h"
#include "palMutex.h"
#include "palPlatform.h"
#include "palSpan.h"
#include "palUtil.h"
#include "palVector.h"

namespace Pal
{
struct ComputePipelineCreateInfo;
class Device;
class IPipeline;
class IShaderLibrary;

// =====================================================================================================================
// A ref-counted ELF loaded as either IPipeline or IShaderLibrary.  Managed by PipelineLoader.
class LoadedElf
{
public:
    // Constructor/destructor.
    LoadedElf(Device* pDevice, uint64 hash, uint64 origHash);
    ~LoadedElf();

    // Initialize (load the ELF and set the ref count to 1).
    Result Init(const ComputePipelineCreateInfo& createInfo, Util::Span<LoadedElf* const> otherElfs);
    Result Init(const GraphicsPipelineCreateInfo& createInfo);
    Result Init(const ShaderLibraryCreateInfo& createInfo);

    Result ResolveRelocs();

    // Get the hash supplied on creation.
    uint64 Hash()     const { return m_hash;     }
    uint64 OrigHash() const { return m_origHash; }
    // Find exported symbol in this loaded ELF.
    bool FindSymbol(const char* pName, uint64* pValue) const;

    // Get the underlying IPipeline, or nullptr if it is a shader library.
    IPipeline* GetPipeline() const { return m_pPipeline; }

    // Get the underlying IShaderLibrary, or nullptr if it is a pipeline.
    IShaderLibrary* GetShaderLibrary() const { return m_pShaderLibrary; }

private:
    // Increment reference count.  This is unsafe; the caller needs to protect it with a mutex.
    void Ref() { ++m_refCount; }

    // Decrement reference count and return new value.  This is unsafe; the caller needs to protect it with a mutex.
    uint32 Deref() { PAL_ASSERT(m_refCount != 0); return --m_refCount; }

    using SymbolVector = Util::Vector<Util::Elf::SymbolTableEntry, 8, IPlatform>;

    Device*         m_pDevice;

    uint64          m_origHash;              // Hash (from archive member name)
    uint64          m_hash;                  // Hash (modified by other pipeline args, used as map key)

    IPipeline*      m_pPipeline;
    IShaderLibrary* m_pShaderLibrary;
    char*           m_pSymStr;
    SymbolVector    m_symbols;

    uint32         m_refCount;

    // This is needed so that PipelineLoader can call Ref()/Deref(), which are only intended to be called from there.
    friend class PipelineLoader;
};

// =====================================================================================================================
// Class for loading an archive pipeline of multiple ELFs with cross-ELF relocs.
// Currently only supports new path ray-tracing pipelines.
class PipelineLoader
{
public:
    // Get size of PipelineLoader object
    static size_t GetSize() { return sizeof(PipelineLoader); }

    PipelineLoader(Device* pDevice);
    ~PipelineLoader();

    // Initialize PipelineLoader object
    Result Init();

    // Get the Device
    Device* GetDevice() const { return m_pDevice; }

    // Find an already-loaded ELF, or load it: compute pipeline/library edition
    Result GetElf(uint64                            origHash,
                  const ComputePipelineCreateInfo&  createInfo,
                  Util::Span<LoadedElf* const>      otherElfs,
                  LoadedElf**                       ppLoadedElf);

    // Find an already-loaded ELF, or load it: graphics pipeline edition
    Result GetElf(uint64                            origHash,
                  const GraphicsPipelineCreateInfo& createInfo,
                  LoadedElf**                       ppLoadedElf);

    // Find an already-loaded ELF, or load it: compute/graphics library edition
    Result GetElf(uint64                         origHash,
                  const ShaderLibraryCreateInfo& createInfo,
                  LoadedElf**                    ppLoadedElf);

    // Release a loaded ELF, freeing it if it is the last reference.
    void ReleaseLoadedElf(LoadedElf* pLoadedElf);

private:
    // Find an already-loaded ELF, or load it using the supplied callback function.
    Result FindOrLoadElf(uint64                                hash,
                         uint64                                origHash,
                         Util::FunctionRef<Result(LoadedElf*)> LoadCallback,
                         LoadedElf**                           ppLoadedElf);

    using LoadedElfMap = Util::HashMap<uint64, LoadedElf*, IPlatform>;

    Device*       m_pDevice;
    LoadedElfMap  m_loadedElfs;      // Map from hash to loaded ELF
    Util::Mutex   m_loadedElfsMutex; // Mutex for loaded ELFs map and ref counting
};

} // Pal
