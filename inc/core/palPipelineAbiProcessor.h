/*
 *******************************************************************************
 *
 * Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

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

namespace Util
{
namespace Abi
{

/// The PipelineAbiProcessor simplifies creating and loading ELFs compatible with the pipeline ABI.
template <typename Allocator>
class PipelineAbiProcessor
{

typedef HashMap<
    uint32,
    RegisterEntry,
    Allocator,
    JenkinsHashFunc,
    DefaultEqualFunc,
    HashAllocator<Allocator>,
    PAL_CACHE_LINE_BYTES * 2> RegisterMap;

typedef HashMapEntry<uint32, RegisterEntry> RegisterMapEntry;
typedef HashIterator<
    uint32,
    RegisterMapEntry,
    Allocator,
    JenkinsHashFunc<uint32>,
    DefaultEqualFunc<uint32>,
    HashAllocator<Allocator>,
    PAL_CACHE_LINE_BYTES * 2> RegisterMapIter;

typedef VectorIterator<PipelineMetadataEntry, 16, Allocator> PipelineMetadataVectorIter;
typedef Vector<PipelineMetadataEntry, 16, Allocator>         PipelineMetadataVector;

typedef VectorIterator<PipelineSymbolEntry, 8, Allocator> PipelineSymbolVectorIter;
typedef Vector<PipelineSymbolEntry, 8, Allocator>         PipelineSymbolVector;

public:
    explicit PipelineAbiProcessor(Allocator* const pAllocator);

    /// Add a RegisterEntry.
    ///
    /// @param [in] entry The RegisterEntry to add.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddRegisterEntry(RegisterEntry entry)
        { return m_registerMap.Insert(entry.key, entry); }

    /// Add a register entry by specifying the register offset and value.
    ///
    /// @param [in] offset Offset of the hardware register to add.
    /// @param [in] value  Value of the hardware register to add.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddRegisterEntry(uint32 offset, uint32 value);

    /// Add a PipelineMetadataEntry.
    ///
    /// @param [in] entry The PipelineMetadataEntry to add.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddPipelineMetadataEntry(PipelineMetadataEntry entry);

    /// Add a PipelineSymbolEntry.
    ///
    /// @param [in] entry The PipelineSymbolEntry to add.
    ///
    /// @returns Success if successful, otherwise ErrorOutOfMemory if memory allocation fails.
    Result AddPipelineSymbolEntry(PipelineSymbolEntry entry);

    /// Set the GFXIP version.
    ///
    /// @param [in] gfxipMajorVer The major version.
    /// @param [in] gfxipMinorVer The minor version.
    /// @param [in] gfxipStepping The stepping.
    void SetGfxIpVersion(
        uint32 gfxipMajorVer,
        uint32 gfxipMinorVer,
        uint32 gfxipStepping);

    /// Set the pipeline shader code.
    ///
    /// @param [in] pCode           Pointer to the pipeline shader code.
    /// @param [in] codeSize        The size of the pipeline shader code in bytes.
    ///
    /// @returns Success if successful, ErrorOutOfMemory if memory allocation fails.
    Result SetPipelineCode(
        const void* pCode,
        size_t      codeSize);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    Result SetPipelineCode(
        const void* pCode,
        size_t      codeSize,
        gpusize     gpuVirtLoadAddr)
    { return SetPipelineCode(pCode, codeSize); }
#endif

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    Result SetData(
        const void* pData,
        size_t      dataSize,
        gpusize     gpuVirtLoadAddr,
        gpusize     alignment)
    { return SetData(pData, dataSize, alignment); }
#endif

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    Result SetReadOnlyData(
        const void* pData,
        size_t      dataSize,
        gpusize     gpuVirtLoadAddr,
        gpusize     alignment)
    { return SetReadOnlyData(pData, dataSize, alignment); }
#endif

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

    /// Check if a RegisterEntry exists.
    ///
    /// @param [in] registerOffset The register to check.
    ///
    /// @returns If the RegisterEntry exists.
    bool HasRegisterEntry(uint32 registerOffset) const
        { return (m_registerMap.FindKey(registerOffset) != nullptr); }

    /// Check if a RegisterEntry exists and return it through an output parameter.
    ///
    /// @param [in]  registerOffset The register to check.
    /// @param [out] pRegisterValue The register value set if the register exists.
    ///
    /// @returns If the RegisterEntry exists.
    bool HasRegisterEntry(uint32 registerOffset, uint32* pRegisterValue) const;

    /// Get the associated RegisterEntry value.
    ///
    /// @param [in] registerOffset The register to get.
    ///
    /// @returns A RegisterEntry value.
    uint32 GetRegisterEntry(uint32 registerOffset) const;

    /// Check if a PipelineMetadataEntry exists.
    ///
    /// @param [in] pipelineMetadataType The PipelineMetadataType to check.
    ///
    /// @returns If the PipelineMetadataEntry exists.
    bool HasPipelineMetadataEntry(PipelineMetadataType pipelineMetadataType) const
        { return m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataType)] >= 0; }

    /// Check if a PipelineMetadataEntry exists and return it through an output parameter.
    ///
    /// @param [in]  pipelineMetadataType The PipelineMetadataType to check.
    /// @param [out] pPipelineMetadataValue The value set if the PipelineMetadataType exists.
    ///
    /// @returns If the PipelineMetadataType exists.
    bool HasPipelineMetadataEntry(
        PipelineMetadataType pipelineMetadataType,
        uint32*              pPipelineMetadataValue) const;

    /// Check if the two PipelineMetadataEntries exist.
    ///
    /// @param [in] pipelineMetadataTypeHigh The PipelineMetadataType to check which will represent
    ///             the high 32-bits of the returned value.
    /// @param [in] pipelineMetadataTypeLow The PipelineMetadataType to check which will represent
    ///             the low 32-bits of the returned value.
    ///
    /// @returns If the PipelineMetadataTypes exists.
    bool HasPipelineMetadataEntries(
        PipelineMetadataType pipelineMetadataTypeHigh,
        PipelineMetadataType pipelineMetadataTypeLow) const;

    /// Check if the two PipelineMetadataEntries exist and return them through an output parameter.
    ///
    /// @param [in] pipelineMetadataTypeHigh The PipelineMetadataType to check which will represent
    ///             the high 32-bits of the returned value.
    /// @param [in] pipelineMetadataTypeLow The PipelineMetadataType to check which will represent
    ///             the low 32-bits of the returned value.
    /// @param [out] pPipelineMetadataValue A uint64 composed of two PipelineMetadataEntry values.
    ///
    /// @returns If the PipelineMetadataTypes exists.
    bool HasPipelineMetadataEntries(
        PipelineMetadataType pipelineMetadataTypeHigh,
        PipelineMetadataType pipelineMetadataTypeLow,
        uint64*              pPipelineMetadataValue) const;

    /// Get the associated PipelineMetadataEntry value.
    ///
    /// @param [in] pipelineMetadataType The PipelineMetadataType to get.
    ///
    /// @returns A PipelineMetadataEntry value.
    uint32 GetPipelineMetadataEntry(PipelineMetadataType pipelineMetadataType) const
    {
        PAL_ASSERT(m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataType)] >= 0);
        return m_pipelineMetadataVector.At(m_pipelineMetadataIndices[static_cast<uint32>(pipelineMetadataType)]).value;
    }

    /// Get the associated PipelineMetadataEntry.
    ///
    /// @param [in] pipelineMetadataTypeHigh The PipelineMetadataType to get which will represent
    ///             the high 32-bits of the returned value.
    /// @param [in] pipelineMetadataTypeLow The PipelineMetadataType to get which will represent
    ///             the low 32-bits of the returned value.
    ///
    /// @returns A uint64 composed of two PipelineMetadataEntry values.
    uint64 GetPipelineMetadataEntries(
        PipelineMetadataType pipelineMetadataTypeHigh,
        PipelineMetadataType pipelineMetadataTypeLow) const;

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

    /// Get the pipeline shader code.
    ///
    /// @param [out] ppCode           Pointer to the pipeline shader code.
    /// @param [out] pCodeSize        The size of the pipeline shader code in bytes.
    void GetPipelineCode(
        const void** ppCode,
        size_t*      pCodeSize) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    void GetPipelineCode(
        const void** ppCode,
        size_t*      pCodeSize,
        gpusize*     pGpuVirtAddr) const
    {
        GetPipelineCode(ppCode, pCodeSize);
        *pGpuVirtAddr = 0;
    }
#endif

    /// Get the pipeline data.
    ///
    /// @param [out] ppData           Pointer to the pipeline data.
    /// @param [out] pDataSize        The size of the pipeline data in bytes.
    /// @param [out] pAlignment       The alignment of the pipeline data.
    void GetData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pAlignment) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    void GetData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pGpuVirtAddr,
        gpusize*     pAlignment) const
    {
        GetData(ppData, pDataSize, pAlignment);
        *pGpuVirtAddr = 0;
    }
#endif

    /// Get the pipeline read only data.
    ///
    /// @param [out] ppData           Pointer to the pipeline read only data.
    /// @param [out] pDataSize        The size of the pipeline read only data in bytes.
    /// @param [out] pAlignment       The alignment of the pipeline read only data.
    void GetReadOnlyData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pAlignment) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 346
    void GetReadOnlyData(
        const void** ppData,
        size_t*      pDataSize,
        gpusize*     pGpuVirtAddr,
        gpusize*     pAlignment) const
    {
        GetReadOnlyData(ppData, pDataSize, pAlignment);
        *pGpuVirtAddr = 0;
    }
#endif

    /// Get the comment which contains compiler version info.
    ///
    /// @returns The comment if it exists, otherwise "".
    const char* GetComment() const;

    /// Gets the pipeline's disassembly data, if it is present.  The disassembly data contains a series of
    /// null-terimated strings (one per shader stage), each of which is a text representation of that stage's
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

    /// Get the GFXIP version.
    ///
    /// @param [out] pGfxipMajorVer The major version.
    /// @param [out] pGfxipMinorVer The minor version.
    /// @param [out] pGgfxipStepping The stepping.
    void GetGfxIpVersion(
        uint32* pGfxipMajorVer,
        uint32* pGfxipMinorVer,
        uint32* pGfxipStepping) const;

    /// Get the ABI version.
    ///
    /// @param [out] pAbiMajorVer The major version.
    /// @param [out] pAbiMinorVer The minor version.
    void GetAbiVersion(
        uint32* pAbiMajorVer,
        uint32* pAbiMinorVer) const;

    /// Get the human-readable pipeline name from the ELF binary.  This was either supplied to the compiler during
    /// compilation, or was not added at all.
    ///
    /// @returns The human-readable pipeline name, or nullptr if no name was added to the ELF binary.
    const char* GetPipelineName() const;

    /// Get the symbol type when given a symbol name.
    ///
    /// @param [in] pName The symbol name.
    ///
    /// @returns The corresponding PipelineSymbolType.
    PipelineSymbolType GetSymbolTypeFromName(const char* pName) const;

    /// Get an iterator at the beginning of the register map.
    ///
    /// @returns An iterator at the beginning of the register map.
    RegisterMapIter RegistersBegin() const
        { return m_registerMap.Begin(); }

    /// Get an iterator at the beginning of the pipeline symbols vector.
    ///
    /// @returns An iterator at the beginning of the pipeline symbols vector.
    PipelineSymbolVectorIter PipelineSymbolsBegin() const
        { return m_pipelineSymbolsVector.Begin(); }

    /// Get an iterator at the beginning of the pipeline metadata vector.
    ///
    /// @returns An iterator at the beginning of the pipeline metadata vector.
    PipelineMetadataVectorIter PipelineMetadataBegin() const
        { return m_pipelineMetadataVector.Begin(); }

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
    /// @returns Returns Success if successful, otherwise ErrorOutOfMemory if memory allocation failed.
    Result Finalize(const char* pPipelineName);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 343
    Result Finalize() { return Finalize(nullptr); }
#endif

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

    AbiAmdGpuVersionNote   m_gpuVersionNote;         // GPU version info.
    AbiMinorVersionNote    m_abiMinorVersionNote;    // ABI minor version version.

    RegisterMap            m_registerMap;            // Register entries

    PipelineMetadataVector m_pipelineMetadataVector; // Pipeline metadata entries
    int32 m_pipelineMetadataIndices[static_cast<uint32>(PipelineMetadataType::Count)];

    PipelineSymbolVector   m_pipelineSymbolsVector;  // Pipeline symbols
    int32 m_pipelineSymbolIndices[static_cast<uint32>(PipelineSymbolType::Count)];

    Elf::ElfProcessor<Allocator> m_elfProcessor;

    Allocator* const m_pAllocator;

    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineAbiProcessor<Allocator>);
};

} //Abi
} //Pal
