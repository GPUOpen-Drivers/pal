
/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palVectorImpl.h"

namespace Pal
{

class CmdBuffer;
class CmdStream;
class PipelineUploader;

// Represents information about shader operations stored obtained as shader metadata flags during processing of shader
// IL stream.
union ShaderMetadataFlags
{
    struct
    {
        uint8 writesUav : 1;   // Shader writes UAVs.
        uint8 reserved  : 7;  // Reserved for future use.
    };
    uint8 u8All;              // All the flags as a single uint.
};

// Represents per-shader metadata, obtained during processing of shader IL.
struct ShaderMetadata
{
    ShaderMetadataFlags flags[NumShaderTypes];
};

// Contains information about each API shader contained in a pipeline.
struct ShaderStageInfo
{
    // Which hardware stage the shader runs on.  Note that multiple API shaders may map to the same hardware stage
    // on some GPU's.
    Util::Abi::HardwareStage  stageId;

    size_t  codeLength;         // Length of the shader's code instructions, in bytes.
    size_t  disassemblyLength;  // Length of the shader's disassembly data, in bytes.
};

// Contains stage information calculated at pipeline bind time.
struct DynamicStageInfo
{
    uint32 wavesPerSh;
    uint32 cuEnableMask;
};

// Identifies what type of pipeline is described by a serialized pipeline ELF.
enum PipelineType : uint32
{
    PipelineTypeUnknown  = 0,
    PipelineTypeCompute  = 1,
    PipelineTypeGraphics = 2,
};

// Describes a symbol that is in GPU memory.
struct GpuSymbol
{
    gpusize gpuVirtAddr;  // The address of the symbol on the GPU.
    gpusize size;         // The size of the symbol.
};

// Contains performance data information for a specific hardware stage.
struct PerfDataInfo
{
    uint32 regOffset;
    size_t cpuOffset;
    uint32 gpuVirtAddr; // Low 32 bits of the gpu virtual address.
    size_t sizeInBytes;
};

// Shorthand for a pipeline ABI reader.
typedef Util::Abi::PipelineAbiReader AbiReader;

// Shorthand for the PAL code object metadata structure.
typedef Util::Abi::PalCodeObjectMetadata  CodeObjectMetadata;

// =====================================================================================================================
// Monolithic object containing all shaders and a large amount of "shader adjacent" state.  Separate concrete
// implementations will support compute or graphics pipelines.
class Pipeline : public IPipeline
{
public:
    virtual ~Pipeline();

    void DestroyInternal();
    virtual void Destroy() override { this->~Pipeline(); }

    virtual const PipelineInfo& GetInfo() const override { return m_info; }

    virtual Result QueryAllocationInfo(
        size_t*                    pNumEntries,
        GpuMemSubAllocInfo* const  pAllocInfoList) const override;

    virtual Result GetShaderCode(
        ShaderType shaderType,
        size_t*    pSize,
        void*      pBuffer) const override;

    virtual Result GetCodeObject(
        uint32*  pSize,
        void*    pBuffer) const override;

    virtual Result GetPerformanceData(
        Util::Abi::HardwareStage hardwareStage,
        size_t*                  pSize,
        void*                    pBuffer) override;

    virtual Result LinkWithLibraries(
        const IShaderLibrary*const* ppLibraryList,
        uint32                      libraryCount) override;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 580
    virtual void SetStackSizeInBytes(
        uint32 stackSizeInBytes) override;
#endif

    virtual Util::Abi::ApiHwShaderMapping ApiHwShaderMapping() const override
        { return m_apiHwMapping; }

    UploadFenceToken GetUploadFenceToken() const { return m_uploadFenceToken; }

protected:
    Pipeline(Device* pDevice, bool isInternal);

    bool IsInternal() const { return m_flags.isInternal != 0; }

    Result PerformRelocationsAndUploadToGpuMemory(
        const CodeObjectMetadata& metadata,
        const GpuHeap&            clientPreferredHeap,
        PipelineUploader*         pUploader);

    void ExtractPipelineInfo(
        const CodeObjectMetadata& metadata,
        ShaderType                firstShader,
        ShaderType                lastShader);

    // Obtains a structure describing the traits of the hardware shader stage associated with a particular API shader
    // type.  Returns nullptr if the shader type is not present for the current pipeline.
    virtual const ShaderStageInfo* GetShaderStageInfo(ShaderType shaderType) const = 0;

    Result GetShaderStatsForStage(
        const ShaderStageInfo& stageInfo,
        const ShaderStageInfo* pStageInfoCopy,
        ShaderStats*           pStats) const;

    void DumpPipelineElf(
        const char*         pPrefix,
        const char*         pName) const;

    size_t PerformanceDataSize(
        const CodeObjectMetadata& metadata) const;

    Device*const  m_pDevice;

    PipelineInfo    m_info;             // Public info structure available to the client.
    ShaderMetadata  m_shaderMetaData;   // Metadata flags for each shader type.

    BoundGpuMemory  m_gpuMem;
    gpusize         m_gpuMemSize;

    void*   m_pPipelineBinary;      // Buffer containing the pipeline binary data (Pipeline ELF ABI).
    size_t  m_pipelineBinaryLen;    // Size of the pipeline binary data, in bytes.

    PerfDataInfo m_perfDataInfo[static_cast<size_t>(Util::Abi::HardwareStage::Count)];
    Util::Abi::ApiHwShaderMapping m_apiHwMapping;

    UploadFenceToken m_uploadFenceToken;
private:
    union
    {
        struct
        {
            uint32  isInternal       :  1;  // True if this Pipeline object was created internally by PAL.
            uint32  reserved         : 31;
        };
        uint32  value;  // Flags packed as a uint32.
    } m_flags;

    BoundGpuMemory m_perfDataMem;
    gpusize        m_perfDataGpuMemSize;

    PAL_DISALLOW_DEFAULT_CTOR(Pipeline);
    PAL_DISALLOW_COPY_AND_ASSIGN(Pipeline);
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
        const void*                pCpuLocalAddr) :
        m_sectionId(sectionId),
        m_gpuVirtAddr(gpuVirtAddr),
        m_pCpuLocalAddr(pCpuLocalAddr),
        m_allocator(pAllocator),
        m_chunks(&m_allocator)
    {}
    SectionInfo(const SectionInfo& other) :
        m_sectionId(other.m_sectionId),
        m_gpuVirtAddr(other.m_gpuVirtAddr),
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

    Util::ElfReader::SectionId GetSectionId() const { return m_sectionId; }
    gpusize GetGpuVirtAddr() const { return m_gpuVirtAddr; }
    const void* GetCpuLocalAddr() const { return m_pCpuLocalAddr; }

    Result AddCpuMappedChunk(const SectionChunk &chunk) { return m_chunks.PushBack(chunk); }

private:
    Util::ElfReader::SectionId m_sectionId;
    // Address of the section in the GPU virtual memory.
    gpusize m_gpuVirtAddr;
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
    SectionInfo* AddSection(Util::ElfReader::SectionId sectionId, gpusize gpuVirtAddr, const void* pCpuLocalAddr);

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
    SectionsIter GetSectionsEnd() const { return m_sections.End(); }
    uint64 GetAlignment() const { return m_alignment; }
    gpusize GetSize() const { return m_size; }

private:
    uint64 m_alignment;
    gpusize m_size;

    Util::IndirectAllocator m_allocator;
    // A pipeline usually has one or two sections that get uploaded to the GPU.
    Util::Vector<SectionOffset, 2, Util::IndirectAllocator> m_sections;

    PAL_DISALLOW_COPY_AND_ASSIGN(SectionAddressCalculator);
};

// =====================================================================================================================
// Helper class used for uploading pipeline data from an ELF binary into GPU memory for later execution.
class PipelineUploader
{
public:
    PipelineUploader(
        Device*          pDevice,
        const AbiReader& abiReader,
        uint32           ctxRegisterCount,
        uint32           shRegisterCount);
    virtual ~PipelineUploader();

    Result Begin(
        const CodeObjectMetadata& metadata,
        const GpuHeap&            preferredHeap);

    Result ApplyRelocations();

    Result End(UploadFenceToken* pCompletionFence);

    uint32 ShRegisterCount() const { return m_shRegisterCount; }
    uint32 CtxRegisterCount() const { return m_ctxRegisterCount; }

    bool EnableLoadIndexPath() const { return ((CtxRegisterCount() + ShRegisterCount()) != 0); }

    GpuMemory* GpuMem() const { return m_pGpuMemory; }
    gpusize GpuMemSize() const { return m_gpuMemSize; }
    gpusize GpuMemOffset() const { return m_baseOffset; }

    gpusize CtxRegGpuVirtAddr() const { return m_ctxRegGpuVirtAddr; }
    gpusize ShRegGpuVirtAddr() const { return m_shRegGpuVirtAddr; }

    gpusize PrefetchAddr() const { return m_prefetchGpuVirtAddr; }
    gpusize PrefetchSize() const { return m_prefetchSize; }

    // Get the address of a pipeline symbol on the GPU.
    Result GetPipelineGpuSymbol(
        Util::Abi::PipelineSymbolType type,
        GpuSymbol*                    pSymbol
        ) const;
    // Get the address of a generic symbol on the GPU.
    Result GetGenericGpuSymbol(
        const char* pName,
        GpuSymbol*  pSymbol
        ) const;

protected:
    Result ApplyRelocationSection(const Util::ElfReader::Relocations& relocations);

    // Writes a context register offset and value to the mapped region where registers are stored in GPU memory.
    PAL_INLINE void AddCtxRegister(uint16 offset, uint32 value)
    {
        m_pCtxRegWritePtr[0] = offset;
        m_pCtxRegWritePtr[1] = value;
        m_pCtxRegWritePtr   += 2;
    }

    // Writes an SH register offset and value to the mapped region where registers are stored in GPU memory.
    PAL_INLINE void AddShRegister(uint16 offset, uint32 value)
    {
        m_pShRegWritePtr[0] = offset;
        m_pShRegWritePtr[1] = value;
        m_pShRegWritePtr   += 2;
    }

private:
    Result UploadPipelineSections(
        const void*  pSectionBuffer,
        size_t       sectionBufferSize,
        SectionInfo* pChunks);

    void PatchPipelineInternalSrdTable(Util::ElfReader::SectionId dataSectionId);

    Result GetAbsoluteSymbolAddress(
        const Util::Elf::SymbolTableEntry* pElfSymbol,
        GpuSymbol*                         pSymbol
        ) const;

    Device*const m_pDevice;
    const AbiReader& m_abiReader;

    GpuMemory*  m_pGpuMemory;
    gpusize     m_baseOffset;
    gpusize     m_gpuMemSize;

    // GpuMemory objects of embedded data allocations used for uploading pipelines to local invis heap.
    GpuMemory* m_pUploadGpuMem;
    gpusize    m_uploadOffset;

    gpusize     m_prefetchGpuVirtAddr;
    gpusize     m_prefetchSize;

    SectionMemoryMap m_memoryMap;
    gpusize  m_ctxRegGpuVirtAddr;
    gpusize  m_shRegGpuVirtAddr;

    const uint32  m_shRegisterCount;
    const uint32  m_ctxRegisterCount;

    void*    m_pMappedPtr;
    uint32*  m_pCtxRegWritePtr;
    uint32*  m_pShRegWritePtr;
#if PAL_ENABLE_PRINTS_ASSERTS
    uint32*  m_pCtxRegWritePtrStart;
    uint32*  m_pShRegWritePtrStart;
#endif
    GpuHeap  m_pipelineHeapType; // The heap type where this pipeline is located.
    DmaUploadRing* m_pDmaUploadRing;
    UploadRingSlot m_slotId;
    gpusize m_heapInvisUploadOffset;

    PAL_DISALLOW_DEFAULT_CTOR(PipelineUploader);
    PAL_DISALLOW_COPY_AND_ASSIGN(PipelineUploader);
};

} // Pal
