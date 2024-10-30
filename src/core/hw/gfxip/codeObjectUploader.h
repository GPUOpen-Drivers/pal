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
#include "palPipeline.h"
#include "palPipelineAbiReader.h"
#include "palSparseVectorImpl.h"
#include "palStringView.h"
#include "palVectorImpl.h"

namespace Pal
{

// GPU memory alignment for shader program sectionss.
constexpr gpusize GpuSectionMemByteAlign = 256;

// Describes a symbol that is in GPU memory.
struct GpuSymbol
{
    gpusize gpuVirtAddr;  // The address of the symbol on the GPU.
    gpusize size;         // The size of the symbol.
};

struct SectionChunk
{
    // The CPU address where the GPU memory is mapped.
    // For host invisible memory, this is the address of the temporary CPU copy in the dma queue.
    void*   pCpuMappedAddr;
    gpusize size;
};

// Shorthand for a pipeline ABI reader.
using AbiReader = Util::Abi::PipelineAbiReader;

// =====================================================================================================================
class SectionInfo
{
using SectionId = Util::ElfReader::SectionId;
public:
    template <typename Allocator>
    SectionInfo(
        Allocator*  pAllocator,
        uint32      elfIndex,
        SectionId   sectionId,
        gpusize     gpuVirtAddr,
        gpusize     offset,
        const void* pCpuLocalAddr)
        :
        m_elfIndex(elfIndex),
        m_sectionId(sectionId),
        m_gpuVirtAddr(gpuVirtAddr),
        m_offset(offset),
        m_pCpuLocalAddr(pCpuLocalAddr),
        m_allocator(pAllocator),
        m_chunks(&m_allocator)
    {}
    SectionInfo(const SectionInfo& other) :
        m_elfIndex(other.m_elfIndex),
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

    uint32      GetElfIndex()     const { return m_elfIndex;      }
    SectionId   GetSectionId()    const { return m_sectionId;     }
    gpusize     GetGpuVirtAddr()  const { return m_gpuVirtAddr;   }
    gpusize     GetOffset()       const { return m_offset;        }
    const void* GetCpuLocalAddr() const { return m_pCpuLocalAddr; }

    Result AddCpuMappedChunk(const SectionChunk &chunk) { return m_chunks.PushBack(chunk); }

private:
    const uint32 m_elfIndex;
    SectionId    m_sectionId;

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
using SectionId = Util::ElfReader::SectionId;
public:
    template <typename Allocator>
    SectionMemoryMap(Allocator*const pAllocator) :
        m_allocator(pAllocator),
        m_sections(&m_allocator)
    {}

    // The cpu mapped chunks have to be filled afterwards.
    // Returns null if an out of memory error occured.
    SectionInfo* AddSection(
        uint32 elfIndex, SectionId sectionId,
        gpusize gpuVirtAddr, gpusize offset, const void* pCpuLocalAddr);

    uint32 GetNumSections() const { return m_sections.NumElements(); }
    SectionId GetSectionId(uint32 i) const { return m_sections.At(i).GetSectionId(); }

    // Get the GPU virtual address of an ELF's section.
    // Returns null if the given section was not found.
    const SectionInfo* FindSection(uint32 elfIndex, SectionId sectionId) const;

private:
    Util::IndirectAllocator m_allocator;
    // A pipeline usually has one or two sections that get uploaded to the GPU. A multi-ELF pipeline may have some more.
    Util::Vector<SectionInfo, 2, Util::IndirectAllocator> m_sections;

    PAL_DISALLOW_COPY_AND_ASSIGN(SectionMemoryMap);
};

// =====================================================================================================================
// Helper class used to compute addresses of ELF sections in GPU memory.
// Stores a mapping of where sections from pipeline ELF files are mapped into virtual GPU memory.
class SectionAddressCalculator
{
using SectionId = Util::ElfReader::SectionId;
public:
    struct SectionOffset
    {
        uint32    elfIndex;
        SectionId sectionId;
        gpusize   offset;
    };

    typedef Util::VectorIterator<SectionOffset, 2, Util::IndirectAllocator> SectionsIter;

    template <typename Allocator>
    SectionAddressCalculator(Allocator*const pAllocator) :
        m_alignment(1),
        m_size(0),
        m_allocator(pAllocator),
        m_sections(&m_allocator)
    {}

    Result AddSection(const Util::ElfReader::Reader& elfReader, uint32 elfIndex, SectionId sectionId);

    SectionsIter GetSectionsBegin() const { return m_sections.Begin(); }
    SectionsIter GetSectionsEnd()   const { return m_sections.End(); }
    uint64       GetAlignment()     const { return m_alignment; }
    gpusize      GetSize()          const { return m_size; }

private:
    uint64  m_alignment;
    gpusize m_size;

    Util::IndirectAllocator m_allocator;
    // A pipeline usually has 1-2 sections per ELF that get uploaded to the GPU.
    Util::Vector<SectionOffset, 2, Util::IndirectAllocator> m_sections;

    PAL_DISALLOW_COPY_AND_ASSIGN(SectionAddressCalculator);
};

// =====================================================================================================================
// Helper class used for uploading pipeline data from ELF binaries into GPU memory for later execution.
class CodeObjectUploader
{
public:
    CodeObjectUploader(
        Device*          pDevice,
        const AbiReader& abiReader);
    virtual ~CodeObjectUploader();

    Result Begin(GpuHeap heap, const bool isInternal);

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
    Result GetGpuSymbol(
        Util::Abi::PipelineSymbolType type, GpuSymbol* pSymbol) const
            { return GetAbsoluteSymbolAddress(m_abiReader.FindSymbol(type), pSymbol); }

    Result GetGpuSymbol(
        Util::StringView<char> name, GpuSymbol* pSymbol) const
            { return GetAbsoluteSymbolAddress(m_abiReader.FindSymbol(name), pSymbol); }

protected:
    Result ApplyRelocationSection(uint32 elfIndex, const Util::ElfReader::Relocations& relocations);

private:
    Result UploadPipelineSections(
        const void*  pSectionBuffer,
        size_t       sectionBufferSize,
        SectionInfo* pChunks);

    void PatchPipelineInternalSrdTable(uint32 elfIndex, Util::ElfReader::SectionId dataSectionId);

    Result GetAbsoluteSymbolAddress(
        const Util::Abi::SymbolEntry* pElfSymbol,
        GpuSymbol*                    pSymbol) const;

    GpuHeap SelectUploadHeap(GpuHeap heap);

    Result UploadUsingCpu(const SectionAddressCalculator& addressCalc, void** ppMappedPtr);
    Result UploadUsingDma(const SectionAddressCalculator& addressCalc, void** ppMappedPtr);

    Device*const     m_pDevice;
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
