/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2020-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palPipelineAbiReader.h
 * @brief PAL Pipeline ABI utility class implementations. The PipelineAbiReader is a layer on top of ElfReader which
 * loads ELFs compatible with the pipeline ABI.
 ***********************************************************************************************************************
 */

#pragma once

#include "g_palPipelineAbiMetadataImpl.h"
#include "palElfReader.h"
#include "palPipelineAbi.h"
#include "palSpan.h"
#include "palVector.h"
#include "palStringView.h"

namespace Util
{

namespace HsaAbi { class  CodeObjectMetadata; }

namespace Abi
{

/// Corresponds to a pair of {ELF file hash, ELF reader}.  Returned by PipelineAbiReader::GetElfs()
struct ElfEntry
{
    uint64            hash;   ///< Hash ID of the raw ELF file data (incl padders!)  May be 0 for non-archive pipelines.
    ElfReader::Reader reader; ///< ELF reader instance.
};

/// @internal  Used to index to a symbol from some ELF's symbol table.  Usually only consumed by PipelineAbiReader.
struct SymbolEntry
{
    /// The Symbol Table's section ID for this entry in the ELF.  This is not the section ID this symbol points into!
    uint16 m_section;
    /// The index of this symbol in the Symbol Table section
    uint32 m_index;
    /// The archive index of the ELF containing the Symbol Table section.  This is always 0 for non-archive pipelines.
    uint32 m_elfIndex;
};

/// The PipelineAbiReader simplifies loading ELF(s) compatible with the pipeline ABI.
class PipelineAbiReader
{
public:
    typedef Vector<ElfEntry, 3, IndirectAllocator> ElfReaders;

    template <typename Allocator>
    PipelineAbiReader(Allocator* pAllocator, Span<const void> binary);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 906
    /// @deprecated - This back-compat path does not support multi-ELF!
    template <typename Allocator>
    PipelineAbiReader(Allocator* pAllocator, const void* pData) :
        PipelineAbiReader(pAllocator, Span<const void>{ pData, size_t(-1) }) { }
#endif

    ///@{
    /// @returns Success if successful, ErrorInvalidPipelineElf if invalid format.
    Result Init() { return Init(StringView<char>()); }
    Result Init(StringView<char> kernelName);
    ///@}

    /// Gets ELF readers (.reader) and ELF file hashes (.hash) of all ELFs in this pipeline binary.  @ref ElfEntry
    const ElfReaders& GetElfs() const { return m_elfReaders; }

    /// @returns The number of ELFs within this pipeline binary.  For non-archive pipelines, this will only be 1.
    uint32 GetNumElfs() const { return m_elfReaders.NumElements(); }

    /// @returns The ELF "file hash" for the specified ELF index.  For non-archive pipelines, this will always be 0.
    ///
    /// @param [in] index  Index of the ELF in this pipeline binary.
    uint64 GetElfFileHash(uint32 index = 0) { return m_elfReaders[index].hash; }

    ///@{
    /// @returns The ELF reader for the specified ELF index.
    ///
    /// @param [in] index  Index of the ELF in this pipeline binary.
          ElfReader::Reader& GetElfReader(uint32 index = 0)       { return m_elfReaders[index].reader; }
    const ElfReader::Reader& GetElfReader(uint32 index = 0) const { return m_elfReaders[index].reader; }
    ///@}

    /// Get the Pipeline Metadata as a deserialized struct using the given MsgPackReader instance. If successful,
    /// the reader's position will then be moved to either the start of the registers map, or to EOF if there are
    /// no registers.
    ///
    /// @param [in/out] pReader    Pointer to the MsgPackReader to use and (re)init with the metadata blob.
    /// @param [out]    pMetadata  Pointer to where to store the deserialized metadata.
    ///
    /// @returns Success if successful, ErrorInvalidValue, ErrorUnknown or ErrorUnsupportedPipelineElfAbiVersion
    ///          if a parser error occurred, ErrorInvalidPipelineElf if there is no metadata.
    Result GetMetadata(MsgPackReader* pReader, PalAbi::CodeObjectMetadata* pMetadata) const;

    /// Get the Pipeline Metadata as a deserialized class using the given MsgPackReader instance. If successful,
    /// the reader's position will then be moved to either the start of the registers map, or to EOF if there are
    /// no registers.
    ///
    /// @param [in/out] pReader    Pointer to the MsgPackReader to use and (re)init with the metadata blob.
    /// @param [out]    pMetadata  Pointer to where to store the deserialized metadata.
    ///
    /// @returns Success if successful, ErrorInvalidValue, ErrorUnknown or ErrorUnsupportedPipelineElfAbiVersion
    ///          if a parser error occurred, ErrorInvalidPipelineElf if there is no metadata.
    Result GetMetadata(
        MsgPackReader* pReader, HsaAbi::CodeObjectMetadata* pMetadata, StringView<char> kernelName) const;

    /// Gets the high-level OS ABI required by this code object (e.g., ElfOsAbiAmdgpuHsa, ElfOsAbiAmdgpuPal).
    ///
    /// @returns This code object's OS ABI enum.
    uint8 GetOsAbi() const { return m_elfReaders[0].reader.GetHeader().ei_osabi; }

    /// Gets the ABI specific version number (e.g., ElfAbiVersionAmdgpuHsaV2, ElfAbiVersionAmdgpuHsaV3).
    ///
    /// @returns This code object's ABI version.
    uint8 GetAbiVersion() const { return m_elfReaders[0].reader.GetHeader().ei_abiversion; }

    /// Get the GFXIP version.
    ///
    /// @param [out] pGfxIpMajorVer The major version.
    /// @param [out] pGfxIpMinorVer The minor version.
    /// @param [out] pGfxIpStepping The stepping.
    void GetGfxIpVersion(uint32* pGfxIpMajorVer, uint32* pGfxIpMinorVer, uint32* pGfxIpStepping) const;

    ///@{
    /// Gets a pointer to the symbol's data, only valid as long as the input binary is alive.
    /// Convenience functions combining FindSymbol + ElfReader::GetSymbol.
    ///
    /// @param [in] type/name  The PipelineSymbolType (fast path) or generic symbol name string to lookup.
    ///
    /// @returns A const void Span containing the raw symbol data + size, or an empty Span if not found.
    Span<const void> GetSymbol(PipelineSymbolType type) const { return GetSymbol(FindSymbol(type)); }
    Span<const void> GetSymbol(StringView<char>   name) const { return GetSymbol(FindSymbol(name)); }
    ///@}

    ///@{
    /// Makes a new persistent copy of the symbol's data owned by the caller.
    /// Convenience functions combining FindSymbol + ElfReader::CopySymbol.
    /// As with ElfReader::CopySymbol, passing a pSize w/ pBuffer = nullptr returns the required malloc size into pSize.
    ///
    /// @param [in] type/name  The PipelineSymbolType (fast path) or generic symbol name string to lookup.
    ///
    /// @returns Success if symbol was copied successfully, NotFound otherwise.
    Result CopySymbol(PipelineSymbolType type, size_t* pSize, void* pBuffer) const
        { return CopySymbol(FindSymbol(type), pSize, pBuffer); }
    Result CopySymbol(StringView<char> name,   size_t* pSize, void* pBuffer) const
        { return CopySymbol(FindSymbol(name), pSize, pBuffer); }
    ///@}

    ///@{
    /// @returns A pointer to the raw ELF symbol header, or nullptr if the symbol was not found.
    ///
    /// @param [in] type/name  The PipelineSymbolType (fast path) or generic symbol name string to lookup.
    const Elf::SymbolTableEntry* GetSymbolHeader(PipelineSymbolType type) const
        { return GetSymbolHeader(FindSymbol(type)); }
    const Elf::SymbolTableEntry* GetSymbolHeader(StringView<char>   name) const
        { return GetSymbolHeader(FindSymbol(name)); }
    ///@}

    ///@{
    /// Locates which symbol table entry within which ELF's symbol section corresponds to the given symbol identifier.
    /// @see GetSymbol and CopySymbol, which wrap this function.
    ///
    /// @param [in] type/name  The PipelineSymbolType (fast path) or generic symbol name string to lookup.
    ///
    /// @returns A descriptor of where to find this symbol in the appropriate symbol table, or nullptr if not found.
    const SymbolEntry* FindSymbol(PipelineSymbolType type) const;
    const SymbolEntry* FindSymbol(StringView<char>   name) const;
    ///@}

    /// Gets the array of _amdgpu_pipelineLinkN symbols.
    Util::Span<const SymbolEntry> GetPipelineLinkSymbols() const { return m_pipelineLinkSymbols; }

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 906
    /// @deprecated - Use GetSymbolHeader, GetSymbol, or CopySymbol.
    /// Check if a PipelineSymbolEntry exists and return it.
    ///
    /// @param [in]  pipelineSymbolType The PipelineSymbolType to check.
    ///
    /// @returns The found symbol, or nullptr if it was not found.
    const Elf::SymbolTableEntry* GetPipelineSymbol(PipelineSymbolType pipelineSymbolType) const
        { return GetSymbolHeader(pipelineSymbolType); }

    /// @deprecated - Use GetSymbolHeader, GetSymbol, or CopySymbol.
    /// Check if a PipelineSymbolEntry exists and return it.
    ///
    /// @param [in]  name ELF name of the symbol to search for
    ///
    /// @returns The found symbol, or nullptr if it was not found.
    const Elf::SymbolTableEntry* GetGenericSymbol(const StringView<char> name) const
        { return GetSymbolHeader(name); }
#endif

private:
    Result InitCodeObject();
    void   InitDebugValidate() const;
    Result InitSymbolCache(StringView<char> kernelName);

    Span<const void> GetSymbol(const SymbolEntry* pSymbolEntry) const;
    Result CopySymbol(const SymbolEntry* pSymbolEntry, size_t* pSize, void* pBuffer) const;
    const Elf::SymbolTableEntry* GetSymbolHeader(const SymbolEntry* pSymbolEntry) const;

    IndirectAllocator m_allocator;
    ElfReaders        m_elfReaders;

    Span<const void> m_binary;  /// Code object blob.  May be an ELF file, or an archive-of-ELFs file.

    /// The symbols, cached for lookup.
    ///
    /// If the section index of the symbol is 0, it does not exist.
    SymbolEntry m_pipelineSymbols[static_cast<uint32>(PipelineSymbolType::Count)];

    typedef HashMap<uint32,
                    SymbolEntry,
                    IndirectAllocator,
                    DefaultHashFunc,
                    DefaultEqualFunc,
                    HashAllocator<IndirectAllocator>,
                    PAL_CACHE_LINE_BYTES * 2> GenericSymbolMap;

    GenericSymbolMap m_genericSymbolsMap;

    Util::Vector<SymbolEntry, 4, IndirectAllocator> m_pipelineLinkSymbols;
};

// =====================================================================================================================
template <typename Allocator>
PipelineAbiReader::PipelineAbiReader(
    Allocator*        pAllocator,
    Span<const void>  binary)
    :
    m_allocator(pAllocator),
    m_elfReaders(&m_allocator),
    m_binary(binary),
    m_genericSymbolsMap(16u, &m_allocator),
    m_pipelineLinkSymbols(&m_allocator)
{
    PAL_ASSERT(pAllocator != nullptr);
}

} // Abi
} // Util
