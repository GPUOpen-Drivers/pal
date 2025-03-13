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

#include "core/device.h"
#include "core/hw/gfxip/archivePipeline.h"
#include "core/hw/gfxip/pipelineLoader.h"
#include "palUtil.h"
#include "palSpan.h"
#include "palVectorImpl.h"
#include "palHashMapImpl.h"
#include "palDevice.h"
#include "palElf.h"
#include "palGpuMemory.h"
#include "palPipeline.h"
#include "palPipelineAbi.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
PipelineLoader::PipelineLoader(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_loadedElfs(16, pDevice->GetPlatform())
{
}

// =====================================================================================================================
PipelineLoader::~PipelineLoader()
{
    PAL_ASSERT(m_loadedElfs.GetNumEntries() == 0);
}

// =====================================================================================================================
// Initialize PipelineLoader object
Result PipelineLoader::Init()
{
    Result result = m_loadedElfs.Init();

    return result;
}

// =====================================================================================================================
// Find an already-loaded ELF, or load it
Result PipelineLoader::GetElf(
    uint64                           origHash,    // Hash of ELF contents, from archive member name
    const ComputePipelineCreateInfo& createInfo,  // (in) Create info, including ELF contents
    Span<LoadedElf* const>           otherElfs,   // (in) Other ELFs for resolving external symbols in
    LoadedElf**                      ppLoadedElf) // (out) Found or created LoadedElf, with ref count incremented
{
    // Include the parts of createInfo other than the ELF pointer and size in the hash. This is necessary if
    // it is an IPipeline, as the same ELF could be used with different other args in different pipelines.
    // It is not necessary for a IShaderLibrary, but we don't know which it is yet.
    MetroHash64 hasher;
    hasher.Update(origHash);
    hasher.Update(createInfo.flags);
    hasher.Update(createInfo.maxFunctionCallDepth);
    hasher.Update(createInfo.disablePartialDispatchPreemption);
    hasher.Update(createInfo.interleaveSize);
#if PAL_BUILD_GFX12
    hasher.Update(createInfo.groupLaunchGuarantee);
#endif
    uint64 hash = 0;
    hasher.Finalize(reinterpret_cast<uint8*>(&hash));

    // Find already-loaded ELF.
    LoadedElf* pLoadedElf = nullptr;
    {
        MutexAuto lock(&m_loadedElfsMutex);
        LoadedElf** ppEntry = m_loadedElfs.FindKey(hash);
        if (ppEntry != nullptr)
        {
            // Found it. Increment reference count.
            pLoadedElf = *ppEntry;
            pLoadedElf->Ref();
        }
    }

    // If not found, load the ELF. (This should not happen if the member contents in the archive were empty.)
    Result result = Result::Success;
    if (pLoadedElf == nullptr)
    {
        if (createInfo.pipelineBinarySize == 0)
        {
            PAL_ASSERT_ALWAYS();
            result = Result::ErrorInvalidValue;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
            pLoadedElf = PAL_NEW(LoadedElf, m_pDevice->GetPlatform(), AllocInternal)(m_pDevice);
            if (pLoadedElf != nullptr)
            {
                result = pLoadedElf->Init(hash, origHash, createInfo, otherElfs);
                if (result != Result::Success)
                {
                    PAL_DELETE(pLoadedElf, m_pDevice->GetPlatform());
                    pLoadedElf = nullptr;
                }
            }
            if (result == Result::Success)
            {
                // We have loaded the ELF. Find or create a map entry for it.
                MutexAuto lock(&m_loadedElfsMutex);
                bool existed = false;
                LoadedElf** ppEntry = nullptr;
                result = m_loadedElfs.FindAllocate(hash, &existed, &ppEntry);
                if (result == Result::Success)
                {
                    if (existed)
                    {
                        // Someone else loaded the same ELF in the meantime. Free our one and use the other
                        // one, incrementing the reference count.
                        PAL_DELETE(pLoadedElf, m_pDevice->GetPlatform());
                        pLoadedElf = *ppEntry;
                        pLoadedElf->Ref();
                    }
                    else
                    {
                        // Copy our loaded ELF into the map.
                        *ppEntry = pLoadedElf;
                    }
                }
            }
        }
    }

    *ppLoadedElf = pLoadedElf;
    return result;
}

// =====================================================================================================================
// Release a loaded ELF, freeing it if it is the last reference.
void PipelineLoader::ReleaseLoadedElf(
    LoadedElf* pLoadedElf)
{
    MutexAuto lock(&m_loadedElfsMutex);
    if (pLoadedElf->Deref() == 0)
    {
        m_loadedElfs.Erase(pLoadedElf->Hash());
        PAL_DELETE(pLoadedElf, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
LoadedElf::LoadedElf(
    Device* pDevice)
    :
    m_pDevice(pDevice),
    m_pPipeline(nullptr),
    m_pShaderLibrary(nullptr),
    m_pSymStr(nullptr),
    m_symbols(pDevice->GetPlatform()),
    m_refCount(0)
{
}

// =====================================================================================================================
LoadedElf::~LoadedElf()
{
    if (m_pPipeline != nullptr)
    {
        m_pPipeline->Destroy();
        PAL_SAFE_FREE(m_pPipeline, m_pDevice->GetPlatform());
    }
    if (m_pShaderLibrary != nullptr)
    {
        m_pShaderLibrary->Destroy();
        PAL_SAFE_FREE(m_pShaderLibrary, m_pDevice->GetPlatform());
    }
    PAL_SAFE_FREE(m_pSymStr, m_pDevice->GetPlatform());
}

// =====================================================================================================================
// Initialize (load the ELF and set the ref count to 1).
// Passing other already-loaded ELFs like this for symbol resolution relies on the caller knowing the right order
// to load ELFs, and there being no circular references.
Result LoadedElf::Init(
    uint64                           hash,       // Hash (from archive name, modified for other pipeline args)
    uint64                           origHash,   // Original hash (from archive name) to attach
    const ComputePipelineCreateInfo& createInfo, // (in) Struct containing pointer/size of ELF, and other info
    Span<LoadedElf* const>           otherElfs)  // (in) Array of other ELFs for external symbol resolution
{
    m_hash     = hash;
    m_origHash = origHash;

    //  Take a copy of the ELF in case we need to modify it, and start parsing it.
    Span<char> elf(
        static_cast<char*>(PAL_MALLOC(createInfo.pipelineBinarySize, m_pDevice->GetPlatform(), AllocInternal)),
        createInfo.pipelineBinarySize);
    Result result = (elf.Data() == nullptr) ? Result::ErrorOutOfMemory : Result::Success;
    if (result == Result::Success)
    {
        memcpy(elf.Data(), createInfo.pPipelineBinary, elf.NumElements());
    }

    const Elf::FileHeader& fileHeader = *reinterpret_cast<const Elf::FileHeader*>(elf.Data());
    if ((result == Result::Success) && (fileHeader.ei_magic != Elf::ElfMagic))
    {
        PAL_ALERT_ALWAYS_MSG("Is not ELF");
        result = Result::ErrorBadShaderCode;
    }

    // Find symbol table.
    Span<Elf::SectionHeader>    sections;
    Span<Elf::SymbolTableEntry> symbols;
    if (result == Result::Success)
    {
        sections = Span<Elf::SectionHeader>(
                      reinterpret_cast<Elf::SectionHeader*>(&elf[fileHeader.e_shoff]), fileHeader.e_shnum);
        for (const Elf::SectionHeader& unalignedSection : sections)
        {
            Elf::SectionHeader section = {};
            memcpy(&section, &unalignedSection, sizeof(section));
            if (section.sh_type == uint32(Elf::SectionHeaderType::SymTab))
            {
                symbols = Span<Elf::SymbolTableEntry>(
                          reinterpret_cast<Elf::SymbolTableEntry*>(&elf[section.sh_offset]),
                          section.sh_size / sizeof(Elf::SymbolTableEntry));
                Elf::SectionHeader strSection = {};
                memcpy(&strSection, &sections[section.sh_link], sizeof(strSection));
                m_pSymStr = static_cast<char*>(PAL_MALLOC(strSection.sh_size, m_pDevice->GetPlatform(), AllocInternal));
                if (m_pSymStr == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                }
                else
                {
                    memcpy(m_pSymStr, &elf[strSection.sh_offset], strSection.sh_size);
                }
                break;
            }
        }
    }

    // Scan symbols, to:
    // 1. spot if this is a pipeline;
    // 2. remember defined global symbols;
    // 3. resolve external symbols.
    bool isPipeline = false;
    if (result == Result::Success)
    {
        for (Elf::SymbolTableEntry& unalignedSymbol : symbols)
        {
            Elf::SymbolTableEntry* pUnalignedSymbol = &unalignedSymbol;
            Elf::SymbolTableEntry symbol = {};
            memcpy(&symbol, pUnalignedSymbol, sizeof(symbol));
            if (symbol.st_info.binding == uint32(Elf::SymbolTableEntryBinding::Local))
            {
                // Ignore non-global symbols.
                continue;
            }
            if (symbol.st_shndx != uint32(Elf::SectionHeaderIndex::Undef))
            {
                // Remember defined global symbol. For now, it is not resolved to an actual GPU address;
                // we fix that later.
                result = m_symbols.PushBack(symbol);
                if (result != Result::Success)
                {
                    break;
                }
                // See if it is a pipeline. Currently, we only check if it's a compute shader.
                if ((isPipeline == false) && (
                    strcmp(m_pSymStr + symbol.st_name,
                           Abi::PipelineAbiSymbolNameStrings[uint32(Abi::PipelineSymbolType::CsMainEntry)])
                    == 0))
                {
                    isPipeline = true;
                }
            }
            else
            {
                // Undefined symbol. Resolve it.
                // The compiler generates an undefined symbol with a prefix that is the hash of the other ELF then a colon.
                // The compiler is now changing such that the same prefix is also on the definition of the symbol in the
                // other ELF, thus the prefix no longer has any semantics here, and we can just search all other
                // ELFs for it.
                // For now, we need to cope with both schemes.
                bool found = false;
                const char* pName = m_pSymStr + symbol.st_name;
                const char* pNameAfterPrefix = pName;
                char* pEnd = nullptr;
                uint64 otherHash = strtoull(pName, &pEnd, 16);
                if (*pEnd != ':')
                {
                    otherHash = 0;
                }
                else
                {
                    pNameAfterPrefix = pEnd + 1;
                }
                for (const LoadedElf* pOtherElf : otherElfs)
                {
                    if (pOtherElf->FindSymbol(pName, &symbol.st_value) ||
                        ((otherHash == pOtherElf->OrigHash()) &&
                         pOtherElf->FindSymbol(pNameAfterPrefix, &symbol.st_value)))
                    {
                        // Resolved it.
                        found = true;
                        symbol.st_shndx = uint32(Elf::SectionHeaderIndex::Abs);
                        memcpy(pUnalignedSymbol, &symbol, sizeof(symbol));
                        break;
                    }
                }
                if (found == false)
                {
                    // Failed to resolve it.
                    PAL_ALERT_ALWAYS_MSG("Failed to resolve symbol %s", m_pSymStr + symbol.st_name);
                    result = Result::ErrorBadShaderCode;
                    break;
                }
            }
        }
    }

    // Apply abs32/abs64 reloc using an abs symbol now, and remove such relocs from the ELF. (This covers the case
    // of a resolved external symbol above.)
    if (result == Result::Success)
    {
        for (Elf::SectionHeader& relSect : sections)
        {
            Elf::SectionHeader* pRelSect = &relSect;
            if (pRelSect->sh_type != uint32(Elf::SectionHeaderType::Rel))
            {
                // Ignore non-rel section.
                continue;
            }
            const Elf::SectionHeader& dataSect = sections[pRelSect->sh_info];
            Span<char> data(&elf[dataSect.sh_offset], dataSect.sh_size);
            const Elf::SectionHeader& symSect = sections[pRelSect->sh_link];
            Span<const Elf::SymbolTableEntry> syms(
                                  reinterpret_cast<const Elf::SymbolTableEntry*>(&elf[symSect.sh_offset]),
                                  symSect.sh_size / sizeof(Elf::SymbolTableEntry));
            Span<Elf::RelTableEntry> rels(
                                  reinterpret_cast<Elf::RelTableEntry*>(&elf[relSect.sh_offset]),
                                  uint32(relSect.sh_size / sizeof(Elf::RelTableEntry)));

            uint32 writeIdx = 0;
            for (uint32 readIdx = 0; readIdx != rels.NumElements(); ++readIdx)
            {
                Elf::RelTableEntry rel = {};
                memcpy(&rel, &rels[readIdx], sizeof(rel));
                if (rel.r_info.sym != 0)
                {
                    Elf::SymbolTableEntry sym = {};
                    memcpy(&sym, &syms[rel.r_info.sym], sizeof(rel));
                    if (sym.st_shndx == uint32(Elf::SectionHeaderIndex::Abs))
                    {
                        switch (Abi::RelocationType(rel.r_info.type))
                        {
                        case Abi::RelocationType::Abs64:
                            {
                                // This is an abs64 reloc to an abs symbol. We can resolve it now.
                                uint64 value = 0;
                                memcpy(&value, &data[rel.r_offset], sizeof(value));
                                value += sym.st_value;
                                memcpy(&data[rel.r_offset], &value, sizeof(value));
                                // Discard this reloc.
                                continue;
                            }
                        case Abi::RelocationType::Abs32:
                        case Abi::RelocationType::Abs32Lo:
                            {
                                // This is an abs32 reloc to an abs symbol. We can resolve it now.
                                uint32 value = 0;
                                memcpy(&value, &data[rel.r_offset], sizeof(value));
                                value += uint32(sym.st_value);
                                memcpy(&data[rel.r_offset], &value, sizeof(value));
                                // Discard this reloc.
                                continue;
                            }
                        case Util::Abi::RelocationType::Abs32Hi:
                            {
                                // This is an abs32hi reloc to an abs symbol. We can resolve it now.
                                uint32 value = 0;
                                memcpy(&value, &data[rel.r_offset], sizeof(value));
                                value += uint32(sym.st_value >> 32);
                                memcpy(&data[rel.r_offset], &value, sizeof(value));
                                // Discard this reloc.
                                continue;
                            }
                        default:
                            break;
                        }
                    }
                }
                // Otherwise, keep the reloc.
                memcpy(&rels[writeIdx++], &rel, sizeof(rel));
            }
            // Update the size of the reloc section.
            pRelSect->sh_size = writeIdx * sizeof(Elf::RelTableEntry);
        }
    }

    if (result == Result::Success)
    {
        for (const Elf::SymbolTableEntry& symbol : m_symbols)
        {
        }
    }

    // Now load the ELF, either as a compute pipeline or as a shader library.
    GpuMemSubAllocInfo gpuMemAlloc({});
    if (result == Result::Success)
    {
        if (isPipeline)
        {
            // Loading as a compute pipeline. Set up a local ComputePipelineCreateInfo with our copy of the ELF,
            // possibly modified above.
            ComputePipelineCreateInfo localInfo = createInfo;
            localInfo.pPipelineBinary = elf.Data();
            localInfo.pipelineBinarySize = elf.NumElements();

            // Create the pipeline.
            size_t size = m_pDevice->GetComputePipelineSize(localInfo, nullptr);
            void* pMem = PAL_MALLOC(size, m_pDevice->GetPlatform(), AllocInternal);
            result = Result::ErrorOutOfMemory;
            if (pMem != nullptr)
            {
                result = m_pDevice->CreateComputePipeline(localInfo, pMem, &m_pPipeline);
                if (result != Result::Success)
                {
                    PAL_FREE(pMem, m_pDevice->GetPlatform());
                }
            }

            // Get the GPU address of the ShfAlloc sections.
            // This relies on internal information about the PAL ELF loader: it puts all ShfAlloc sections into
            // one GPU memory allocation.
            if (result == Result::Success)
            {
                size_t numAllocs = 1;
                result = m_pPipeline->QueryAllocationInfo(&size, &gpuMemAlloc);
            }
        }
        else
        {
            // Loading as a shader library. We don't set up any function names for resolution here;
            // to actually resolve the symbol(s) in the loaded ELF, we use the same hack code below as for
            // a compute pipeline.
            ShaderLibraryCreateInfo localInfo = {};
            localInfo.pCodeObject    = elf.Data();
            localInfo.codeObjectSize = elf.NumElements();

            // Create the shader library.
            size_t size = m_pDevice->GetShaderLibrarySize(localInfo, nullptr);
            void* pMem = PAL_MALLOC(size, m_pDevice->GetPlatform(), AllocInternal);
            result = Result::ErrorOutOfMemory;
            if (pMem != nullptr)
            {
                result = m_pDevice->CreateShaderLibrary(localInfo, pMem, &m_pShaderLibrary);
                if (result != Result::Success)
                {
                    PAL_FREE(pMem, m_pDevice->GetPlatform());
                }
            }

            // Get the GPU address of the ShfAlloc sections.
            // This relies on internal information about the PAL ELF loader: it puts all ShfAlloc sections into
            // one GPU memory allocation.
            if (result == Result::Success)
            {
                size_t numAllocs = 1;
                result = m_pShaderLibrary->QueryAllocationInfo(&size, &gpuMemAlloc);
            }
        }
    }

    // Resolve the exported symbols by getting the GPU addresses of loaded sections.
    // This relies on internal information about the PAL ELF loader: it puts all ShfAlloc sections into
    // one GPU memory allocation in the order that the sections appear in the ELF, with the specified
    // alignment.
    // First calculate the offset of each section.
    Vector<uint64, 8, Platform> sectionOffsets(m_pDevice->GetPlatform());
    if (result == Result::Success)
    {
        result = sectionOffsets.Resize(static_cast<uint32>(sections.NumElements()));
    }
    if (result == Result::Success)
    {
        uint64 offset = 0;
        for (uint32 idx = 0; idx != sections.NumElements(); ++idx)
        {
            const Elf::SectionHeader& section = sections[idx];
            if ((section.sh_flags & Elf::SectionHeaderFlags::ShfAlloc) != 0)
            {
                if (section.sh_addralign != 0)
                {
                    offset = Pow2Align(offset, section.sh_addralign);
                }
                sectionOffsets[idx] = offset;
                offset += section.sh_size;
            }
        }
        // Scan the symbols and resolve absolute ones in a ShfAlloc section (loaded into GPU memory).
        for (Elf::SymbolTableEntry& symbol : m_symbols)
        {
            Elf::SymbolTableEntry* pSymbol = &symbol;
            if ((pSymbol->st_shndx == 0) || (pSymbol->st_shndx >= sections.NumElements()))
            {
                continue; // Ignore symbol that is not defined in a section
            }
            const Elf::SectionHeader& section = sections[pSymbol->st_shndx];
            if ((section.sh_flags & Elf::SectionHeaderFlags::ShfAlloc) == 0)
            {
                continue; // Ignore symbol in section not loaded into GPU memory
            }
            pSymbol->st_value += gpuMemAlloc.address;
            pSymbol->st_value += gpuMemAlloc.offset;
            pSymbol->st_value += sectionOffsets[pSymbol->st_shndx];
            pSymbol->st_shndx = uint32(Elf::SectionHeaderIndex::Abs);
        }
    }

    // Free our copy of the ELF.
    PAL_FREE(elf.Data(), m_pDevice->GetPlatform());

    if (result == Result::Success)
    {
        m_refCount = 1;
    }
    return result;
}

// =====================================================================================================================
// Find exported symbol in this loaded ELF. Returns true if the symbol was found.
bool LoadedElf::FindSymbol(
    const char* pName, // (in) Symbol name
    uint64*     pValue // (out) Symbol value
    ) const
{
    bool found = false;
    for (const Elf::SymbolTableEntry& symbol : m_symbols)
    {
        const char* pSymName = m_pSymStr + symbol.st_name;
        if (strcmp(pSymName, pName) == 0)
        {
            *pValue = symbol.st_value;
            found = true;
            break;
        }
    }
    return found;
}

}
