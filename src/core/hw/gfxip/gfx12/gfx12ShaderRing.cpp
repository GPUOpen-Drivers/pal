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
#include "core/hw/gfxip/gfx12/gfx12Chip.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRing.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
ShaderRing::ShaderRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           tmzEnabled,
    ShaderRingType type)
    :
    m_pDevice(pDevice),
    m_pSrdTable(pSrdTable),
    m_ringMem(),
    m_tmzEnabled(tmzEnabled),
    m_allocSize(0),
    m_numMaxWaves(0),
    m_itemSizeMax(0),
    m_ringType(type)
{
}

// =====================================================================================================================
ShaderRing::~ShaderRing()
{
    // The ShaderRing class does not own the memory for VertexAttributes, PrimBuffer and PosBuffer
    if (m_ringMem.IsBound()                              &&
        (m_ringType != ShaderRingType::VertexAttributes) &&
        (m_ringType != ShaderRingType::PrimBuffer)       &&
        (m_ringType != ShaderRingType::PosBuffer))
    {
        m_pDevice->Parent()->MemMgr()->FreeGpuMem(m_ringMem.Memory(), m_ringMem.Offset());
    }
}

// =====================================================================================================================
// Computes the video memory allocation size based on the number of parallel wavefronts allowed to execute in HW and the
// largest item size currently seen.  Returns the allocation size, in bytes.
gpusize ShaderRing::ComputeAllocationSize() const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    // The size to allocate for this Ring is: threadsPerWavefront * maxWaves * itemSize DWORDs.
    return (chipProps.gfx9.maxWavefrontSize * m_numMaxWaves * m_itemSizeMax * sizeof(uint32));
}

// =====================================================================================================================
Result ShaderRing::AllocateVideoMemory(
    gpusize           memorySizeBytes,
    ShaderRingMemory* pDeferredMem)
{
    Pal::Device*const pParent = m_pDevice->Parent();
    InternalMemMgr*const pMemMgr = pParent->MemMgr();

    if (m_ringMem.IsBound())
    {
        // Store m_ringMem for later cleanup.
        pDeferredMem->pGpuMemory = m_ringMem.Memory();
        pDeferredMem->offset     = m_ringMem.Offset();
        m_ringMem.Update(nullptr, 0);
    }

    // Alignment requirement for shader rings is 256 Bytes.
    constexpr gpusize ShaderRingAlignment = 256;

    GpuMemoryInternalCreateInfo internalInfo = {};
    internalInfo.flags.alwaysResident = 1;

    GpuMemory* pGpuMemory = nullptr;
    gpusize    memOffset  = 0;
    gpusize*   pMemOffset = &memOffset;

    GpuMemoryCreateInfo createInfo = { };
    createInfo.size      = memorySizeBytes;
    createInfo.alignment = ShaderRingAlignment;
    createInfo.priority  = GpuMemPriority::Normal;

    createInfo.flags.tmzProtected = m_tmzEnabled;
    createInfo.heapCount          = GetPreferredHeaps(pParent, createInfo.heaps);

    // Allocate video memory for this ring.
    Result result = pMemMgr->AllocateGpuMem(createInfo, internalInfo, 0, &pGpuMemory, pMemOffset);

    if (result == Result::Success)
    {
        m_ringMem.Update(pGpuMemory, memOffset);
    }

    return result;
}

// =====================================================================================================================
// Performs submit-time validation on this shader ring so that any dirty state can be updated.
Result ShaderRing::Validate(
    size_t            itemSize, // Item size of the Ring to validate against (in DWORDs)
    ShaderRingMemory* pDeferredMem)
{
    Result result = Result::Success;

    // Only need to validate if the new item size is larger than the largest we've validated thus far.
    if (itemSize > m_itemSizeMax)
    {
        m_itemSizeMax = itemSize;
        const gpusize sizeNeeded = ComputeAllocationSize();

        // Attempt to allocate the video memory for this Ring.
        result = AllocateVideoMemory(sizeNeeded, pDeferredMem);

        if (result == Result::Success)
        {
            // Track our current allocation size.
            m_allocSize = sizeNeeded;
        }

        if (m_ringMem.IsBound())
        {
            // Update our SRD(s) if the Ring video memory exists.
            UpdateSrds();
        }
    }

    return result;
}

// =====================================================================================================================
uint32 ShaderRing::GetPreferredHeaps(
    const Pal::Device* pDevice,
    GpuHeap*           pHeaps
    ) const
{
    uint32 heapCount = 2;

    pHeaps[0] = GpuHeapInvisible;
    pHeaps[1] = GpuHeapLocal;

    if ((pDevice->GetPublicSettings()->forceShaderRingToVMem == false) ||
        (pDevice->ChipProperties().gpuType == GpuType::Integrated))
    {
        pHeaps[2] = GpuHeapGartUswc;
        heapCount = 3;
    }

    return heapCount;
}

// =====================================================================================================================
static void InitBufferSrd(
    uint32         numRecords,
    uint32         stride,
    BUF_FMT        format,
    sq_buf_rsrc_t* pSrd)
{
    memset(pSrd, 0, sizeof(*pSrd));

    pSrd->base_address      = 0;
    pSrd->stride            = stride;
    pSrd->swizzle_enable    = 3;
    pSrd->num_records       = numRecords;
    pSrd->dst_sel_x         = SQ_SEL_X;
    pSrd->dst_sel_y         = SQ_SEL_Y;
    pSrd->dst_sel_z         = SQ_SEL_Z;
    pSrd->dst_sel_w         = SQ_SEL_W;
    pSrd->format            = format;
    pSrd->index_stride      = BUF_INDEX_STRIDE_32B;
    pSrd->add_tid_enable    = 0;
    pSrd->oob_select        = SQ_OOB_NUM_RECORDS_0;
    pSrd->type              = SQ_RSRC_BUF;
}

// =====================================================================================================================
ScratchRing::ScratchRing(
    Device*         pDevice,
    sq_buf_rsrc_t*  pSrdTable,
    bool            isCompute,
    bool            isTmz)
    :
    ShaderRing(pDevice,
               pSrdTable,
               isTmz,
               isCompute ? ShaderRingType::ComputeScratch : ShaderRingType::GfxScratch),
    m_isCompute(isCompute),
    m_numTotalCus(0),
    m_scratchWaveSizeGranularityShift(0),
    m_scratchWaveSizeGranularity(0)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    m_scratchWaveSizeGranularityShift = ScratchWaveSizeGranularityShift;
    m_scratchWaveSizeGranularity      = (1 << ScratchWaveSizeGranularityShift);

    m_numTotalCus = chipProps.gfx9.numShaderEngines *
                    chipProps.gfx9.numShaderArrays  *
                    chipProps.gfx9.numCuPerSh;

    // The max we expect is one scratch wave on every wave slot in every CU.
    m_numMaxWaves = (chipProps.gfx9.numWavesPerSimd * chipProps.gfx9.numSimdPerCu * m_numTotalCus);

    ShaderRingSrd srdTableIndex = ShaderRingSrd::ScratchGraphics;

    if (isCompute)
    {
        srdTableIndex = ShaderRingSrd::ScratchCompute;

        // We must allow for at least as many waves as there are in the largest threadgroup.
        const uint32 maxWaves = chipProps.gfxip.maxThreadGroupSize / chipProps.gfx9.minWavefrontSize;
        m_numMaxWaves         = Max<size_t>(m_numMaxWaves, maxWaves);
    }

    // The hardware can only support a limited number of scratch waves per CU so make sure we don't exceed that number.
    m_numMaxWaves = Min<size_t>(m_numMaxWaves, (MaxScratchWavesPerCu * m_numTotalCus));
    PAL_ASSERT(m_numMaxWaves <= 0xFFF); // Max bits allowed in reg field, should never hit this.

    sq_buf_rsrc_t*const   pGenericSrd = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    InitBufferSrd(0, 0, BUF_FMT_32_FLOAT, pGenericSrd);

    pGenericSrd->swizzle_enable       = 1;
    pGenericSrd->index_stride         = BUF_INDEX_STRIDE_64B;
    pGenericSrd->add_tid_enable       = 1;
}

// =====================================================================================================================
// Calculates the maximum number of waves that can be in flight on the hardware when scratch is in use.
size_t ScratchRing::CalculateWaves() const
{
    size_t numWaves = m_numMaxWaves;

    // We should only restrict the number of scratch waves if we're actually using scratch.
    if (m_itemSizeMax > 0)
    {
        const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
        const size_t waveSize              = AdjustScratchWaveSize(m_itemSizeMax * chipProps.gfx9.minWavefrontSize);

        // Attempt to allow as many waves in parallel as possible, but make sure we don't launch more waves than we
        // can handle in the scratch ring.
        size_t allocSize   = static_cast<size_t>(m_allocSize) / chipProps.gfx9.numShaderEngines;
        size_t numMaxWaves = m_numMaxWaves / chipProps.gfx9.numShaderEngines;

        numWaves = Min(static_cast<size_t>(allocSize) / (waveSize * sizeof(uint32)), numMaxWaves);
    }

    // Max bits allowed in reg field, should never hit this.
    PAL_ASSERT(numWaves <= 0xFFF);

    return numWaves;
}

// =====================================================================================================================
// Calculates the the wave size for the particular shader type of this ring. Returns the amount of space used by each
// wave in DWORDs.
size_t ScratchRing::CalculateWaveSize() const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    return AdjustScratchWaveSize(m_itemSizeMax * chipProps.gfx9.minWavefrontSize) >> m_scratchWaveSizeGranularityShift;
}

// =====================================================================================================================
// Helper function to make sure the scratch wave size (in dwords) doesn't exceed the register's maximum value
size_t ScratchRing::AdjustScratchWaveSize(
    size_t scratchWaveSize
    ) const
{
    const size_t minWaveSize        = (scratchWaveSize > 0) ? m_scratchWaveSizeGranularity : 0;
    size_t       adjScratchWaveSize =
        (scratchWaveSize > 0) ? RoundUpToMultiple(scratchWaveSize, m_scratchWaveSizeGranularity) : scratchWaveSize;

    // If the size per wave is sufficiently large, and the access pattern of scratch memory only uses a very small,
    // upfront portion of the total amount allocated, we run into an issue where accesses to this scratch memory across
    // all waves fall into the same memory channels, since the memory channels are based on bits [11:8] of the full byte
    // address. Unfortunately, since scratch wave allocation is based on units of 256DW (1KB), this means that that only
    // bits [11:10] really impact the memory channels, and of those we only really care about bit 10.
    // In order to fix this, we try to bump the allocation up by a single unit (256DW) to make each wave more likely to
    // access disparate memory channels.
    // NOTE: For use cases that use low amounts of scratch, this may increase the size of the scratch ring by 50%.
    adjScratchWaveSize |= minWaveSize;

    return Max(Min(MaxScratchWaveSizeInDwords, adjScratchWaveSize), minWaveSize);
}

// =====================================================================================================================
// Overrides the base class' method for computing the scratch buffer size.
gpusize ScratchRing::ComputeAllocationSize() const
{
    const Pal::Device*         pParent   = m_pDevice->Parent();
    const GpuChipProperties&   chipProps = m_pDevice->Parent()->ChipProperties();
    const PalSettings&         settings  = m_pDevice->Parent()->Settings();

    // Compute the adjusted scratch size required by each wave.
    const size_t waveSize = AdjustScratchWaveSize(m_itemSizeMax * chipProps.gfx9.minWavefrontSize);

    // The ideal size to allocate for this Ring is: threadsPerWavefront * maxWaves * itemSize DWORDs.
    // We clamp this allocation to a maximum size to prevent the driver from using an unreasonable amount of scratch.
    const gpusize totalLocalMemSize =
        pParent->HeapLogicalSize(GpuHeapLocal) + pParent->HeapLogicalSize(GpuHeapInvisible);
    const gpusize maxScaledSize     = (settings.maxScratchRingSizeScalePct * totalLocalMemSize) / 100;
    const gpusize maxSize           = Max(settings.maxScratchRingSizeBaseline, maxScaledSize);
    const gpusize allocationSize    = static_cast<gpusize>(m_numMaxWaves) * waveSize * sizeof(uint32);

    return Min(allocationSize, maxSize);
}

// =====================================================================================================================
void ScratchRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const ShaderRingSrd srdTableIndex = m_isCompute ? ShaderRingSrd::ScratchCompute :
                                                      ShaderRingSrd::ScratchGraphics;
    const gpusize       addr          = m_ringMem.GpuVirtAddr();
    sq_buf_rsrc_t*const pSrd          = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    pSrd->base_address = addr;
    pSrd->num_records  = MemorySizeBytes();
}

// =====================================================================================================================
VertexAttributeRing::VertexAttributeRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           tmzEnabled)
    :
    ShaderRing(pDevice, pSrdTable, tmzEnabled, ShaderRingType::VertexAttributes)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    sq_buf_rsrc_t*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::VertexAttributes)];

    // Set-up static SRD fields.
    InitBufferSrd(0, Stride, BUF_FMT_32_32_32_32_FLOAT, pSrd);
}

// =====================================================================================================================
Result VertexAttributeRing::AllocateVideoMemory(
    gpusize           memorySizeBytes,
    ShaderRingMemory* pDeferredMem)
{
    m_pDevice->AllocateVertexAttributesMem(m_tmzEnabled);
    const BoundGpuMemory& vertexAttributesMem = m_pDevice->VertexAttributesMem(m_tmzEnabled);

    m_ringMem.Update(vertexAttributesMem.Memory(), vertexAttributesMem.Offset());

    return Result::Success;
}

// =====================================================================================================================
// Overrides the base class's function for computing the ring size.  Returns the allocation size in bytes.
gpusize VertexAttributeRing::ComputeAllocationSize() const
{
    return m_itemSizeMax * m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;
}

// =====================================================================================================================
void VertexAttributeRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    sq_buf_rsrc_t*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::VertexAttributes)];

    pSrd->base_address = gpuVirtAddr;
    pSrd->num_records  = m_allocSize / Stride;
}

// =====================================================================================================================
SamplePosBuffer::SamplePosBuffer(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable, // Pointer to our parent ring-set's SRD table
    bool           isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::SamplePos)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    sq_buf_rsrc_t* const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::SamplePosBuffer)];

    // Set-up static SRD fields.
    InitBufferSrd(0, Stride, BUF_FMT_32_FLOAT, pSrd);
}

// =====================================================================================================================
gpusize SamplePosBuffer::ComputeAllocationSize() const
{
    return sizeof(SamplePatternPalette);
}

// =====================================================================================================================
void SamplePosBuffer::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    sq_buf_rsrc_t* const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::SamplePosBuffer)];

    pSrd->base_address = gpuVirtAddr;
    pSrd->num_records  = m_allocSize / Stride;
}

// =====================================================================================================================
uint32 SamplePosBuffer::GetPreferredHeaps(
    const Pal::Device* pDevice,
    GpuHeap*           pHeaps
    ) const
{
    uint32 heapCount = 1;

    pHeaps[0] = GpuHeapLocal;

    if ((pDevice->GetPublicSettings()->forceShaderRingToVMem == false) ||
        (pDevice->ChipProperties().gpuType == GpuType::Integrated))
    {
        pHeaps[1] = GpuHeapGartUswc;
        heapCount = 2;
    }

    return heapCount;
}

// =====================================================================================================================
void SamplePosBuffer::UploadSamplePatternPalette(
    const SamplePatternPalette& samplePatternPalette)
{
    // Update sample pattern palette buffer when m_ringMem has video memory bound, which also means
    // IDevice::SetSamplePatternPalette is called by client, and CPU visible video memory is allocated.
    if (m_ringMem.IsBound())
    {
        void*  pData  = nullptr;
        Result result = m_ringMem.Map(&pData);

        if (result == Result::Success)
        {
            memcpy(pData, samplePatternPalette, sizeof(SamplePatternPalette));
            m_ringMem.Unmap();
        }
    }
}

// =====================================================================================================================
TfBuffer::TfBuffer(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::TfBuffer)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    sq_buf_rsrc_t* const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TfBuffer)];

    // Set-up static SRD fields:
    InitBufferSrd(0, 0, BUF_FMT_32_FLOAT, pGenericSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the TF buffer size, since the size of the TF buffer is fixed and
// depends on the number of shader engines present. Returns the allocation size, in bytes.
gpusize TfBuffer::ComputeAllocationSize() const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    return (chipProps.gfxip.tessFactorBufferSizePerSe * chipProps.gfx9.numShaderEngines * sizeof(uint32));
}

// =====================================================================================================================
void TfBuffer::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    sq_buf_rsrc_t* const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TfBuffer)];
    pSrd->base_address = gpuVirtAddr;
    pSrd->num_records  = m_allocSize;
}

// =====================================================================================================================
OffChipLds::OffChipLds(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable, // Pointer to our parent ring-set's SRD table
    bool           isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::OffChipLds)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    sq_buf_rsrc_t* const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::OffChipLds)];

    InitBufferSrd(0, 0, BUF_FMT_32_FLOAT, pGenericSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the offchip LDS buffer size, since the size of the offchip LDS buffer
// depends on the number of offchip LDS buffers available to the chip. Returns the allocation size, in bytes.
gpusize OffChipLds::ComputeAllocationSize() const
{
    // Determine the LDS buffer size in DWORD's based on settings.
    const gpusize offchipLdsBufferSizeBytes = m_pDevice->Parent()->ChipProperties().gfxip.offChipTessBufferSize;

    // Our maximum item size represents how many offchip LDS buffers we need space for in total.
    return (offchipLdsBufferSizeBytes * m_itemSizeMax);
}

// =====================================================================================================================
void OffChipLds::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    sq_buf_rsrc_t* const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::OffChipLds)];
    pSrd->base_address = gpuVirtAddr;
    pSrd->num_records  = m_allocSize;
}

// =====================================================================================================================
PrimBufferRing::PrimBufferRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           tmzEnabled)
    :
    ShaderRing(pDevice, pSrdTable, tmzEnabled, ShaderRingType::PrimBuffer)
{
}

// =====================================================================================================================
Result PrimBufferRing::AllocateVideoMemory(
    gpusize           memorySizeBytes,
    ShaderRingMemory* pDeferredMem)
{
    m_pDevice->AllocatePrimBufferMem(m_tmzEnabled);
    const BoundGpuMemory& primBufferMem = m_pDevice->PrimBufferMem(m_tmzEnabled);

    m_ringMem.Update(primBufferMem.Memory(), primBufferMem.Offset());

    return Result::Success;
}

// =====================================================================================================================
// Overrides the base class's function for computing the ring size.  Returns the allocation size in bytes.
gpusize PrimBufferRing::ComputeAllocationSize() const
{
    return m_itemSizeMax;
}

// =====================================================================================================================
PosBufferRing::PosBufferRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           tmzEnabled)
    :
    ShaderRing(pDevice, pSrdTable, tmzEnabled, ShaderRingType::PosBuffer)
{
}

// =====================================================================================================================
Result PosBufferRing::AllocateVideoMemory(
    gpusize           memorySizeBytes,
    ShaderRingMemory* pDeferredMem)
{
    m_pDevice->AllocatePosBufferMem(m_tmzEnabled);
    const BoundGpuMemory& posBufferMem = m_pDevice->PosBufferMem(m_tmzEnabled);

    m_ringMem.Update(posBufferMem.Memory(), posBufferMem.Offset());

    return Result::Success;
}

// =====================================================================================================================
// Overrides the base class's function for computing the ring size.  Returns the allocation size in bytes.
gpusize PosBufferRing::ComputeAllocationSize() const
{
    return m_itemSizeMax;
}

// =====================================================================================================================
PayloadDataRing::PayloadDataRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::PayloadData),
    m_maxNumEntries(Pow2Pad(m_pDevice->Settings().numTsMsDrawEntriesPerSe *
                            pDevice->Parent()->ChipProperties().gfx9.numShaderEngines))
{
    sq_buf_rsrc_t*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::PayloadDataRing)];
    InitBufferSrd(0, PayloadDataEntrySize, BUF_FMT_32_FLOAT, pGenericSrd);
}

// =====================================================================================================================
void PayloadDataRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    sq_buf_rsrc_t*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::PayloadDataRing)];
    pSrd->base_address = gpuVirtAddr;
    pSrd->num_records  = m_allocSize;
}

// =====================================================================================================================
TaskMeshCtrlDrawRing::TaskMeshCtrlDrawRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable)
    :
    ShaderRing(pDevice, pSrdTable, false, ShaderRingType::TaskMeshCtrlDrawRing),
    m_drawRingEntries(Pow2Pad(m_pDevice->Settings().numTsMsDrawEntriesPerSe *
                              pDevice->Parent()->ChipProperties().gfx9.numShaderEngines)),
    m_drawRingTotalBytes(m_drawRingEntries * DrawDataEntrySize)
{
    sq_buf_rsrc_t*const pDrawData = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::DrawDataRing)];
    InitBufferSrd(0, DrawDataEntrySize, BUF_FMT_32_FLOAT, pDrawData);
}

// =====================================================================================================================
void TaskMeshCtrlDrawRing::InitializeControlBufferAndDrawRingBuffer()
{
    ControlBufferLayout controlBuffer = {};

    constexpr uint32 AlignmentBytes = 64;

    gpusize drawRingAddr = GetDrawRingVirtAddr();

    // The draw ring base address must be aligned to 64-bytes.
    PAL_ASSERT(Util::IsPow2Aligned(drawRingAddr, AlignmentBytes));

    // Number of draw ring entries must be a power of 2.
    PAL_ASSERT(Util::IsPowerOfTwo(m_drawRingEntries));

    controlBuffer.numEntries       = m_drawRingEntries;
    controlBuffer.drawRingBaseAddr = drawRingAddr;

    // The first 5 bits are reserved and need to be set to 0.
    PAL_ASSERT((controlBuffer.drawRingBaseAddr & 0x1F) == 0);

    // The "ready" bit in each DrawDataRing entry toggles and hence is interpreted differently with each pass over the
    // ring. The interpretation of the ready bit depends on the wptr/rdptr. Ex: For even numbered passes, readyBit = 1
    // indicates ready to GFX. For odd numbered passes, readyBit = 0 indicates ready.
    // The formula for the ready bit written by the taskshader is (readyBit = (wptr / numRingEntries) & 1).
    // The "ready" bits in the zero-initialized draw ring are interpreted as being in "not ready" state.
    controlBuffer.readPtr    = m_drawRingEntries;
    controlBuffer.writePtr   = m_drawRingEntries;
    controlBuffer.deallocPtr = m_drawRingEntries;

    // Map and upload the control buffer layout and draw data to the ring.
    if (m_ringMem.IsBound())
    {
        void*  pData  = nullptr;
        const Result result = m_ringMem.Map(&pData);
        if (result == Result::Success)
        {
            memcpy(pData, &controlBuffer, sizeof(ControlBufferLayout));
            // Map and zero-initialize the draw data ring, to ensure a correct initial state of "ready" bits.
            memset(VoidPtrInc(pData, OffsetOfControlDrawRing), 0, m_drawRingTotalBytes);
            m_ringMem.Unmap();
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
void TaskMeshCtrlDrawRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    sq_buf_rsrc_t*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::DrawDataRing)];
    pGenericSrd->base_address = GetDrawRingVirtAddr();
    pGenericSrd->num_records  = m_drawRingTotalBytes;
}

// =====================================================================================================================
uint32 TaskMeshCtrlDrawRing::GetPreferredHeaps(
    const Pal::Device* pDevice,
    GpuHeap*           pHeaps
    ) const
{
    uint32 heapCount = 1;

    pHeaps[0] = GpuHeapLocal;

    if ((pDevice->GetPublicSettings()->forceShaderRingToVMem == false) ||
        (pDevice->ChipProperties().gpuType == GpuType::Integrated))
    {
        pHeaps[1] = GpuHeapGartUswc;
        heapCount = 2;
    }

    return heapCount;
}

// =====================================================================================================================
MeshScratchRing::MeshScratchRing(
    Device*        pDevice,
    sq_buf_rsrc_t* pSrdTable,
    bool           isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::MeshScratch),
    m_maxThreadgroupsPerChip(1 << CountSetBits(VGT_GS_MAX_WAVE_ID__MAX_WAVE_ID_MASK))
{
    sq_buf_rsrc_t*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::MeshScratch)];

    InitBufferSrd(0, 0, BUF_FMT_32_FLOAT, pGenericSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the mesh shader scratch buffer size.
gpusize MeshScratchRing::ComputeAllocationSize() const
{
    return m_itemSizeMax * m_maxThreadgroupsPerChip;
}

// =====================================================================================================================
void MeshScratchRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const ShaderRingSrd srdTableIndex = ShaderRingSrd::MeshScratch;
    const gpusize       addr          = m_ringMem.GpuVirtAddr();
    uint32*const        pData         = reinterpret_cast<uint32*>(&m_pSrdTable[static_cast<size_t>(srdTableIndex)]);

    // The MeshShader scratch ring is accessed via ORDERED_WAVE_ID, which should be large enough to guarantee that no
    // two threadgroups on the system contain the same ID.
    // This ring is a bit special than the other shader rings. Due to the sizes required per threadgroup, the shader
    // cannot properly index using the SRD's stride bits. In order to accommodate this, we write data into the global
    // table in place of an SRD that SC can then use to create an SRD and properly calculate an offset into it.
    pData[0] = LowPart(addr);
    pData[1] = HighPart(addr);
    pData[2] = static_cast<uint32>(MemorySizeBytes());
    pData[3] = static_cast<uint32>(m_itemSizeMax);
}

} // namespace Gfx12
} // namespace Pal
