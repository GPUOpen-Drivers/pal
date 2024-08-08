/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palElfPackager.h"
#include "palElfReader.h"
#include "palLib.h"
#include "palMetroHash.h"
#include "palPipelineAbiReader.h"
#include "palSparseVectorImpl.h"
#include "palStringView.h"
#include "palVectorImpl.h"

namespace Pal
{

class CodeObjectUploader;

// A Graphics Pipeline Library has 3 children: ABI metadata, pre-rasterization shader code/data, and PS code/data.
constexpr uint32 MaxGfxShaderLibraryCount = 3;

// Shorthand for a pipeline ABI reader.
using AbiReader = Util::Abi::PipelineAbiReader;

// Describes a symbol that is in GPU memory.
struct GpuSymbol
{
    gpusize gpuVirtAddr;  // The address of the symbol on the GPU.
    gpusize size;         // The size of the symbol.
};

// =====================================================================================================================
struct SectionChunk
{
    // The CPU address where the GPU memory is mapped.
    // For host invisible memory, this is the address of the temporary CPU copy in the dma queue.
    void* pCpuMappedAddr;
    gpusize size;
};

// =====================================================================================================================
class SectionInfo
{
public:
    template <typename Allocator>
    SectionInfo(
        Allocator*const            pAllocator,
        Util::ElfReader::SectionId sectionId,
        gpusize                    gpuVirtAddr,
        gpusize                    offset,
        const void*                pCpuLocalAddr) :
        m_sectionId(sectionId),
        m_gpuVirtAddr(gpuVirtAddr),
        m_offset(offset),
        m_pCpuLocalAddr(pCpuLocalAddr),
        m_allocator(pAllocator),
        m_chunks(&m_allocator)
    {}
    SectionInfo(const SectionInfo& other) :
        m_sectionId(other.m_sectionId),
        m_gpuVirtAddr(other.m_gpuVirtAddr),
        m_offset(other.m_offset),
        m_pCpuLocalAddr(other.m_pCpuLocalAddr),
        m_allocator(&other.m_allocator),
        m_chunks(&m_allocator)
    {
        for (uint32 i = 0; i < other.m_chunks.NumElements(); i++)
        {
            Result result = m_chunks.PushBack(other.m_chunks.At(i));
            PAL_ASSERT_MSG(result == Result::Success, "Failed to allocate memory to copy vector");
        }
    }

    void* GetCpuMappedAddr(gpusize offset) const;

    Util::ElfReader::SectionId GetSectionId()    const { return m_sectionId;     }
    gpusize                    GetGpuVirtAddr()  const { return m_gpuVirtAddr;   }
    gpusize                    GetOffset()       const { return m_offset;        }
    const void*                GetCpuLocalAddr() const { return m_pCpuLocalAddr; }

    Result AddCpuMappedChunk(const SectionChunk &chunk) { return m_chunks.PushBack(chunk); }

private:
    Util::ElfReader::SectionId m_sectionId;
    // Address of the section in the GPU virtual memory.
    gpusize m_gpuVirtAddr;

    // Offset of the section in the GPU virtual memory.
    gpusize m_offset;

    // Address of the section on the CPU. Refers to the ELF file.
    const void* m_pCpuLocalAddr;

    Util::IndirectAllocator m_allocator;

    // The CPU address where the GPU memory is mapped.
    // For host invisible memory, this is the address of the temporary CPU copy in the dma queue.
    // The dma queue buffer can be split in multiple parts, so we may need to jump between them.
    Util::Vector<SectionChunk, 1, Util::IndirectAllocator> m_chunks;
};

// =====================================================================================================================
class SectionMemoryMap
{
public:
    template <typename Allocator>
    SectionMemoryMap(Allocator*const pAllocator) :
        m_allocator(pAllocator),
        m_sections(&m_allocator)
    {}

    // The cpu mapped chunks have to be filled afterwards.
    // Returns null if an out of memory error occured.
    SectionInfo* AddSection(
        Util::ElfReader::SectionId sectionId,
        gpusize                    gpuVirtAddr,
        gpusize                    offset,
        const void*                pCpuLocalAddr);

    uint32 GetNumSections() const { return m_sections.NumElements(); }
    Util::ElfReader::SectionId GetSectionId(uint32 i) const { return m_sections.At(i).GetSectionId(); }

    // Get the GPU virtual address of a section.
    // Returns null if the given section id was not found.
    const SectionInfo* FindSection(Util::ElfReader::SectionId sectionId) const;

private:
    Util::IndirectAllocator m_allocator;
    // A pipeline usually has one or two sections that get uploaded to the GPU.
    Util::Vector<SectionInfo, 2, Util::IndirectAllocator> m_sections;

    PAL_DISALLOW_COPY_AND_ASSIGN(SectionMemoryMap);
};

// =====================================================================================================================
// Helper class used to compute addresses of ELFsection in GPU memory.
// Stores a mapping of where sections from pipeline ELF files are mapped into virtual GPU memory.
class SectionAddressCalculator
{
public:
    struct SectionOffset
    {
        Util::ElfReader::SectionId sectionId;
        gpusize offset;
    };

    typedef Util::VectorIterator<SectionOffset, 2, Util::IndirectAllocator> SectionsIter;

    template <typename Allocator>
    SectionAddressCalculator(Allocator*const pAllocator) :
        m_alignment(1),
        m_size(0),
        m_allocator(pAllocator),
        m_sections(&m_allocator)
    {}

    Result AddSection(const Util::ElfReader::Reader& elfReader, Util::ElfReader::SectionId sectionId);

    SectionsIter GetSectionsBegin() const { return m_sections.Begin(); }
    SectionsIter GetSectionsEnd()   const { return m_sections.End();   }
    uint64       GetAlignment()     const { return m_alignment;        }
    gpusize      GetSize()          const { return m_size;             }

private:
    uint64 m_alignment;
    gpusize m_size;

    Util::IndirectAllocator m_allocator;
    // A pipeline usually has one or two sections that get uploaded to the GPU.
    Util::Vector<SectionOffset, 2, Util::IndirectAllocator> m_sections;

    PAL_DISALLOW_COPY_AND_ASSIGN(SectionAddressCalculator);
};

// =====================================================================================================================
// Helper class used for uploading pipeline code object data from an ELF binary into GPU memory for later execution.
class CodeObjectUploader
{
public:
    CodeObjectUploader(
        Device*          pDevice,
        const AbiReader& abiReader);
    virtual ~CodeObjectUploader();

    Result Begin(GpuHeap heap, bool isInternal);

    Result ApplyRelocations();

    Result End(UploadFenceToken* pCompletionFence);

    GpuMemory* GpuMem()         const { return m_pGpuMemory; }
    gpusize    GpuMemSize()     const { return m_gpuMemSize; }
    gpusize    GpuMemOffset()   const { return m_baseOffset; }
    gpusize    SectionOffset()  const;
    uint64     PagingFenceVal() const { return m_pagingFenceVal; }

    gpusize PrefetchAddr() const { return m_prefetchGpuVirtAddr; }
    gpusize PrefetchSize() const { return m_prefetchSize; }

    // Get the address of a pipeline symbol on the GPU.
    Result GetPipelineGpuSymbol(
        Util::Abi::PipelineSymbolType type,
        GpuSymbol*                    pSymbol) const;
    // Get the address of a generic symbol on the GPU.
    Result GetGenericGpuSymbol(
        Util::StringView<char> name,
        GpuSymbol*             pSymbol) const;

protected:
    Result ApplyRelocationSection(const Util::ElfReader::Relocations& relocations);

private:
    Result UploadPipelineSections(
        const void*  pSectionBuffer,
        size_t       sectionBufferSize,
        SectionInfo* pChunks);

    void PatchPipelineInternalSrdTable(Util::ElfReader::SectionId dataSectionId);

    Result GetAbsoluteSymbolAddress(
        const Util::Elf::SymbolTableEntry* pElfSymbol,
        GpuSymbol*                         pSymbol) const;

    GpuHeap SelectUploadHeap(GpuHeap heap);

    bool ShouldUploadUsingDma() const { return (m_pipelineHeapType == GpuHeap::GpuHeapInvisible); }

    Result UploadUsingCpu(const SectionAddressCalculator& addressCalc, void** ppMappedPtr);
    Result UploadUsingDma(const SectionAddressCalculator& addressCalc, void** ppMappedPtr);

    Device*const m_pDevice;

    const AbiReader& m_abiReader;

    GpuMemory*  m_pGpuMemory;
    gpusize     m_baseOffset;
    gpusize     m_gpuMemSize;

    gpusize     m_prefetchGpuVirtAddr;
    gpusize     m_prefetchSize;

    SectionMemoryMap m_memoryMap;

    void*    m_pMappedPtr;
    uint64   m_pagingFenceVal;

    GpuHeap         m_pipelineHeapType; // The heap type where this pipeline is located.
    UploadRingSlot  m_slotId;
    gpusize         m_heapInvisUploadOffset;

    PAL_DISALLOW_DEFAULT_CTOR(CodeObjectUploader);
    PAL_DISALLOW_COPY_AND_ASSIGN(CodeObjectUploader);
};

} // Pal
