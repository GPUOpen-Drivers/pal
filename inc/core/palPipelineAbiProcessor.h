/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  palPipelineAbiProcessor.h
 * @brief PAL Pipeline ABI utility class declarations.  The PipelineAbiProcessor is a layer on top of ElfProcessor which
 * creates and loads ELFs compatible with the pipeline ABI.
 ***********************************************************************************************************************
 */

#pragma once

#include "palElfProcessorImpl.h"
#include "palPipelineAbi.h"
#include "g_palPipelineAbiMetadata.h"

namespace Util
{
namespace Abi
{

/// The PipelineAbiProcessor simplifies creating and loading ELFs compatible with the pipeline ABI.
template <typename Allocator>
class PipelineAbiProcessor
{
typedef VectorIterator<PipelineSymbolEntry, 8, Allocator> PipelineSymbolVectorIter;
typedef Vector<PipelineSymbolEntry, 8, Allocator>         PipelineSymbolVector;

typedef HashMap<const char*,
                GenericSymbolEntry,
                Allocator,
                StringJenkinsHashFunc,
                StringEqualFunc,
                HashAllocator<Allocator>,
                PAL_CACHE_LINE_BYTES * 2>   GenericSymbolMap;
typedef typename GenericSymbolMap::Iterator GenericSymbolIter;

public:
    explicit PipelineAbiProcessor(Allocator* const pAllocator);
    ~PipelineAbiProcessor()
    {
        if (m_pAllocator != nullptr)
        {
            PAL_FREE(m_pCompatRegisterBlob, m_pAllocator);
        }
    }

    /// Add a PipelineSymbolEntry.
    ///
    /// @param [in] entry The PipelineSymbolEntry to add.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddPipelineSymbolEntry(PipelineSymbolEntry entry);

    /// Adds a generic symbol to the pipeline binary.  These symbols don't match any of the predetermined symbol
    /// types @ref PipelineSymbolType.
    ///
    /// @param [in] entry The GenericSymbolEntry to add.  This function does not make a memory copy of the pName
    ///                   string for the symbol, so it is up to the caller to make sure that the string memory has
    ///                   at least as long a lifetime as this object has.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddGenericSymbolEntry(GenericSymbolEntry entry);

    /// Set the GFXIP version.
    ///
    /// @param [in] gfxIpMajorVer The major version.
    /// @param [in] gfxIpMinorVer The minor version.
    /// @param [in] gfxIpStepping The stepping.
    void SetGfxIpVersion(
        uint32 gfxIpMajorVer,
        uint32 gfxIpMinorVer,
        uint32 gfxIpStepping);

    /// Set the pipeline shader code.
    ///
    /// @param [in] pCode           Pointer to the pipeline shader code.
    /// @param [in] codeSize        The size of the pipeline shader code in bytes.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetPipelineCode(
        const void* pCode,
        size_t      codeSize);

    /// Set the pipeline data.
    ///
    /// @param [in] pData           Pointer to the pipeline data.
    /// @param [in] dataSize        The size of the pipeline data in bytes.
    /// @param [in] alignment       The alignment of the pipeline data.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetData(
        const void* pData,
        size_t      dataSize,
        gpusize     alignment);

    /// Set the pipeline read only data.
    ///
    /// @param [in] pData           Pointer to the pipeline read only data.
    /// @param [in] dataSize        The size of the pipeline read only data in bytes.
    /// @param [in] alignment       The alignment of the pipeline read only data.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetReadOnlyData(
        const void* pData,
        size_t      dataSize,
        gpusize     alignment);

    /// Set the pipelines' disassembly data.  This should contain disassembly for all shader stages in the
    /// pipeline.  Each shader stage has an associated symbol type which defines the size and offset to the
    /// disassembly data for that stage.
    ///
    /// @param [in] pData     Pointer to the pipeline's disassembly data.  Each shader stage's disassembly
    ///                       data is a null-terminated string.
    /// @param [in] dataSize  Size of the disassembly data, in bytes.  Includes all null terminator(s).
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetDisassembly(
        const void* pData,
        size_t      dataSize);

    /// Set the pipelines' AMDIL data.  This should contain binaries for all shader stages in the
    /// pipeline.  Each shader stage has an associated symbol type which defines the size and offset to the
    /// binary data for that stage.
    ///
    /// @param [in] pData     Pointer to the pipeline's AMDIL binary data.
    /// @param [in] dataSize  Size of the binary data, in bytes.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetAmdIl(
        const void* pData,
        size_t      dataSize);

    /// Set the comment which contains compiler version info.
    ///
    /// @param [in] pComment The comment string.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetComment(const char* pComment);

    /// Check if data has been added.
    ///
    bool HasData() const
        { return (m_pDataSection != nullptr); }

    bool HasReadOnlyData() const
        { return (m_pRoDataSection != nullptr); }

    /// Check if a PipelineSymbolEntry exists.
    ///
    /// @param [in] symbolType The PipelineSymbolType to check.
    ///
    /// @returns If the PipelineSymbolEntry exists.
    bool HasPipelineSymbolEntry(PipelineSymbolType symbolType) const
        { return m_pipelineSymbolIndices[static_cast<uint32>(symbolType)] >= 0; }

    /// Check if a PipelineSymbolEntry exists and return it through an output parameter.
    ///
    /// @param [in]  pipelineSymbolType The PipelineSymbolType to check.
    /// @param [out] pPipelineSymbolEntry The entry if the PipelineSymbolType exists.
    ///
    /// @returns If the PipelineSymbolType exists.
    bool HasPipelineSymbolEntry(
        PipelineSymbolType   pipelineSymbolType,
        PipelineSymbolEntry* pPipelineSymbolEntry) const;

    /// Get the associated PipelineSymbolEntry.
    ///
    /// @param [in] symbolType The PipelineSymbolType to get.
    ///
    /// @returns The PipelineSymbolEntry.
    PipelineSymbolEntry GetPipelineSymbolEntry(PipelineSymbolType pipelineSymbolType) const
    {
        PAL_ASSERT(m_pipelineSymbolIndices[static_cast<uint32>(pipelineSymbolType)] >= 0);
        return m_pipelineSymbolsVector.At(m_pipelineSymbolIndices[static_cast<uint32>(pipelineSymbolType)]);
    }

    /// Check if a GenericSymbolEntry exists and return it through an output parameter.
    ///
    /// @param [in]  pName                ELF name of the symbol to search for
    /// @param [out] pGenericSymbolEntry  The symbol entry, if it exists.
    ///
    /// @returns If the symbol with the given name exists in the ELF.
    bool HasGenericSymbolEntry(const char* pName, GenericSymbolEntry* pGenericSymbolEntry) const;

    /// Get the Pipeline Metadata as a deserialized struct using the given MsgPackReader instance. If successful,
    /// the reader's position will then be moved to either the start of the registers map, or to EOF if there are
    /// no registers.
    ///
    /// @param [in/out] pReader    Pointer to the MsgPackReader to use and (re)init with the metadata blob.
    /// @param [out]    pMetadata  Pointer to where to store the deserialized metadata.
    ///
    /// @returns Result if successful, ErrorInvalidValue if a parser error occurred, ErrorInvalidPipelineElf if
    ///          there is no metadata.
    Result GetMetadata(
        MsgPackReader*         pReader,
        PalCodeObjectMetadata* pMetadata) const;

    /// Get the Pipeline Metadata as a binary blob.
    ///
    /// @param [out] ppMetadata     Pointer to the pipeline metadata.
    /// @param [out] pMetadataSize  The size of the pipeline metadata in bytes.
    void GetMetadata(
        const void** ppMetadata,
        size_t*      pMetadataSize) const;

    /// Get the pipeline shader code.
    ///
    /// @param [out] ppCode           Pointer to the pipeline shader code.
    /// @param [out] pCodeSize        The size of the pipeline shader code in bytes.
    void GetPipelineCode(
        const void** ppCode,
        size_t*      pCodeSize) const;

    /// Get the pipeline data.
    ///
    /// @param [out] ppData           Pointer to the pipeline data.
    /// @param [out] pDataSize        The size of the pipeline data in bytes.
    /// @param [out] pAlignment       The alignment of the pipeline data.
    void GetData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pAlignment) const;

    /// Get the pipeline read only data.
    ///
    /// @param [out] ppData           Pointer to the pipeline read only data.
    /// @param [out] pDataSize        The size of the pipeline read only data in bytes.
    /// @param [out] pAlignment       The alignment of the pipeline read only data.
    void GetReadOnlyData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pAlignment) const;

    /// Get the comment which contains compiler version info.
    ///
    /// @returns The comment if it exists, otherwise "".
    const char* GetComment() const;

    /// Gets the pipeline's disassembly data, if it is present.  The disassembly data contains a series of
    /// null-terimated strings (one per HW shader stage), each of which is a text representation of that stage's
    /// executable shader code.  Each shader stage has an associated symbol type which defines the size and
    /// offset to the disassembly data for that stage.
    ///
    /// @param [out] ppData     Pointer to the disassembly data for the whole pipeline.  Will be nullptr if
    ///                         the disassembly data is not present in the ELF.
    /// @param [out] pDataSize  Size of the disassembly data in bytes.  Will be zero if the disassembly data
    ///                         is not present in the ELF.
    void GetDisassembly(
        const void** ppData,
        size_t*      pDataSize) const;

    /// Gets the pipeline's AMDIL token data, if it is present.  The AMDIL data contains a series of binary
    /// blobs (one per API shader stage), each of which is a binary representation of that API shader's
    /// executable shader IL.  Each API shader stage has an associated symbol type which defines the size and
    /// offset to the disassembly data for that stage.
    ///
    /// @param [out] ppData     Pointer to the AMDIL token data for the whole pipeline.  Will be nullptr if
    ///                         the AMDIL data is not present in the ELF.
    /// @param [out] pDataSize  Size of the AMDIL data in bytes.  Will be zero if the AMDIL data is not present
    ///                         in the ELF.
    void GetAmdIl(
        const void** ppData,
        size_t*      pDataSize) const;

    /// Get the GFXIP version.
    ///
    /// @param [out] pGfxIpMajorVer The major version.
    /// @param [out] pGfxIpMinorVer The minor version.
    /// @param [out] pGfxIpStepping The stepping.
    void GetGfxIpVersion(
        uint32* pGfxIpMajorVer,
        uint32* pGfxIpMinorVer,
        uint32* pGfxIpStepping) const;

    /// Get the Metadata version.
    ///
    /// @param [out] pMajorVer The major version.
    /// @param [out] pMinorVer The minor version.
    void GetMetadataVersion(
        uint32* pMajorVer,
        uint32* pMinorVer) const;

    /// Get the symbol type when given a symbol name.
    ///
    /// @param [in] pName The symbol name.
    ///
    /// @returns The corresponding PipelineSymbolType.
    PipelineSymbolType GetSymbolTypeFromName(const char* pName) const;

    /// Get an iterator at the beginning of the pipeline symbols vector.
    ///
    /// @returns An iterator at the beginning of the pipeline symbols vector.
    PipelineSymbolVectorIter PipelineSymbolsBegin() const
        { return m_pipelineSymbolsVector.Begin(); }

    /// Apply relocations to the Code, Data, or ReadOnly Data.
    ///
    /// @param [in] pBuffer        Pointer to the buffer to apply relocations to.
    /// @param [in] bufferSize     Size of the buffer in bytes to apply relocations to.
    /// @param [in] AbiSectionType The ABI section type to apply relocations to.
    void ApplyRelocations(
        void*          pBuffer,
        size_t         bufferSize,
        AbiSectionType sectionType,
        uint64         baseAddress) const;

    /// Finalizes the ABI filling out all the elf structures. Call this and
    /// make custom changes with the returned ElfProcessor before calling
    /// GetRequiredBufferSizeBytes() and SaveToBuffer().
    ///
    /// @param [in] pipelineMetadataWriter  MsgPack writer containing a blob encoding pipeline metadata.
    ///
    /// @returns Returns Success if successful, otherwise ErrorOutOfMemory if memory allocation failed.
    Result Finalize(const MsgPackWriter& pipelineMetadataWriter);

    /// Returns an ElfProcessor to allow direct ELF queries.
    ///
    /// @returns A pointer to an ElfProcessor to allow additional queries.
    Elf::ElfProcessor<Allocator>* GetElfProcessor() { return &m_elfProcessor; }

    /// Returns an ElfProcessor to allow direct ELF queries.
    ///
    /// @returns A pointer to an ElfProcessor to allow additional queries.
    const Elf::ElfProcessor<Allocator>* GetElfProcessor() const { return &m_elfProcessor; }

    /// Gets the number of bytes required to hold a binary blob of the ELF.
    ///
    /// @returns Returns the size of the ELF in bytes.
    size_t GetRequiredBufferSizeBytes() const
        { return m_elfProcessor.GetRequiredBufferSizeBytes(); }

    /// Save the ELF to a buffer.
    ///
    /// @param [in] pBuffer Pointer to the buffer to save to.
    void SaveToBuffer(void* pBuffer);

    /// Initialize the ABI processor before generating an ELF.  If LoadFromBuffer is not going to be called, then
    /// this must be called instead before any operations can be done on this ELF.
    ///
    /// @returns Success if successful, or ErrorOutOfMemory upon allocation failure.
    Result Init();

    /// Load the ELF from a buffer.
    ///
    /// @param [in] pBuffer    Pointer to the buffer to load from.
    /// @param [in] bufferSize Size of the buffer in bytes to load from.
    Result LoadFromBuffer(const void* pBuffer, size_t bufferSize);

private:
    void RelocationHelper(
        void*                    pBuffer,
        uint64                   baseAddress,
        Elf::Section<Allocator>* pRelocationSection) const;

    Result TranslateLegacyMetadata(MsgPackReader* pReader, PalCodeObjectMetadata* pOut) const;

    Result CreateDataSection();
    Result CreateRoDataSection();
    Result CreateTextSection();

    Elf::Section<Allocator>* m_pTextSection;         // Contains executable machine code for all shader stages.
    Elf::Section<Allocator>* m_pDataSection;         // Data
    Elf::Section<Allocator>* m_pRoDataSection;       // Read only data

    Elf::Section<Allocator>* m_pRelTextSection;      // Rel for text section.
    Elf::Section<Allocator>* m_pRelDataSection;      // Rel for data section.

    Elf::Section<Allocator>* m_pRelaTextSection;     // Rela for text section.
    Elf::Section<Allocator>* m_pRelaDataSection;     // Rela for data section.

    Elf::Section<Allocator>* m_pSymbolSection;       // Symbols
    Elf::Section<Allocator>* m_pSymbolStrTabSection; // Symbol String Table
    Elf::Section<Allocator>* m_pNoteSection;         // Notes: HsaIsa / AbiMinorVersion / PalMetadata
    Elf::Section<Allocator>* m_pCommentSection;      // Comment with compiler info
    Elf::Section<Allocator>* m_pDisasmSection;       // Disassembly section (.AMDGPU.disasm)
    Elf::Section<Allocator>* m_pAmdIlSection;        // AMDIL section (.AMDGPU.comment.amdil)

    AmdGpuElfFlags         m_flags;                  // ELF flags. Contains GPU info, such as GFXIP version.

    uint32                 m_metadataMajorVer;       // Metadata ABI major version.
    uint32                 m_metadataMinorVer;       // Metadata ABI minor version.

    uint32                 m_compatRegisterSize;     // Back-compat: Size of the register MsgPack blob.
    void*                  m_pCompatRegisterBlob;    // Back-compat: MsgPack blob encoding registers.

    const void*            m_pMetadata;              // Pointer to metadata blob.
    size_t                 m_metadataSize;           // Size of the metadata blob in bytes.

    GenericSymbolMap       m_genericSymbolsMap;      // Map of generic symbols
    PipelineSymbolVector   m_pipelineSymbolsVector;  // Pipeline symbols
    int32 m_pipelineSymbolIndices[static_cast<uint32>(PipelineSymbolType::Count)];

    Elf::ElfProcessor<Allocator> m_elfProcessor;

    Allocator* const m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineAbiProcessor<Allocator>);
};

} // Abi
} // Pal
