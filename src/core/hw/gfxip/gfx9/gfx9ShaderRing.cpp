/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/internalMemMgr.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRing.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

constexpr size_t ScratchWaveSizeGranularityShift     = 8;
constexpr size_t ScratchWaveSizeGranularity          = (1ull << ScratchWaveSizeGranularityShift);
#if PAL_BUILD_GFX11
constexpr size_t ScratchWaveSizeGranularityShiftNv31 = 6;
constexpr size_t ScratchWaveSizeGranularityNv31      = (1ull << ScratchWaveSizeGranularityShiftNv31);
#endif

// =====================================================================================================================
// On GFXIP 9 hardware, buffer SRD's which set the ADD_TID_ENABLE bit in word3 changes the meaning of the DATA_FORMAT
// field to stride bits [17:14] used for scratch offset boundary checks instead of the format.
static void AdjustRingDataFormat(
    const GpuChipProperties& chipProps,
    BufferSrd*               pGenericSrd)
{
    if (chipProps.gfxLevel == GfxIpLevel::GfxIp9)
    {
        auto*  pSrd = &pGenericSrd->gfx9;

        if (pSrd->word3.bits.ADD_TID_ENABLE != 0)
        {
            pSrd->word3.bits.DATA_FORMAT = BUF_DATA_FORMAT_INVALID; // Sets the extended stride to zero.
        }
    }
}

// =====================================================================================================================
ShaderRing::ShaderRing(
    Device*        pDevice,
    BufferSrd*     pSrdTable,
    bool           isTmz,
    ShaderRingType type)
    :
    m_pDevice(pDevice),
    m_pSrdTable(pSrdTable),
    m_tmzEnabled(isTmz),
    m_allocSize(0),
    m_numMaxWaves(0),
    m_itemSizeMax(0),
    m_ringType(type),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel)
{
}

// =====================================================================================================================
ShaderRing::~ShaderRing()
{
    if (m_ringMem.IsBound()
#if PAL_BUILD_GFX11
        // The ShaderRing class does not own the memory for VertexAttributes
        && (m_ringType != ShaderRingType::VertexAttributes)
#endif
            )
    {
        m_pDevice->Parent()->MemMgr()->FreeGpuMem(m_ringMem.Memory(), m_ringMem.Offset());
    }
}

// =====================================================================================================================
// Computes the video memory allocation size based on the number of parallel wavefronts allowed to execute in HW and the
// largest item size currently seen. Returns the allocation size, in bytes.
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
    InternalMemMgr*const pMemMgr = m_pDevice->Parent()->MemMgr();

    if (m_ringMem.IsBound())
    {
        // store m_ringMem for later cleanup
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

    if ((m_ringType == ShaderRingType::SamplePos) || (m_ringType == ShaderRingType::TaskMeshCtrlDrawRing))
    {
        // SamplePos doesn't need a TMZ allocation because it's updated by the CPU and only read by the GPU.
        createInfo.heaps[0]  = GpuHeapLocal;
        createInfo.heaps[1]  = GpuHeapGartUswc;
        createInfo.heapCount = 2;
    }
    else
    {
        createInfo.flags.tmzProtected = m_tmzEnabled;
        createInfo.heaps[0]           = GpuHeapInvisible;
        createInfo.heaps[1]           = GpuHeapLocal;
        createInfo.heaps[2]           = GpuHeapGartUswc;
        createInfo.heapCount          = 3;
    }

    // Allocate video memory for this Ring.
    Result result = pMemMgr->AllocateGpuMem(createInfo, internalInfo, 0, &pGpuMemory, pMemOffset);

    if (result == Result::Success)
    {
        m_ringMem.Update(pGpuMemory, memOffset);
    }

    return result;
}

// =====================================================================================================================
// Performs submit-time validation on this shader Ring so that any dirty state can be updated.
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
ScratchRing::ScratchRing(
    Device*       pDevice,
    BufferSrd*    pSrdTable,
    Pm4ShaderType shaderType,
    bool          isTmz)
    :
    ShaderRing(pDevice,
               pSrdTable,
               isTmz,
               (shaderType == ShaderCompute) ? ShaderRingType::ComputeScratch : ShaderRingType::GfxScratch),
    m_shaderType(shaderType),
    m_numTotalCus(0),
    m_scratchWaveSizeGranularityShift(0),
    m_scratchWaveSizeGranularity(0)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

#if PAL_BUILD_GFX11
    if (IsGfx11(m_gfxLevel))
    {
        m_scratchWaveSizeGranularityShift = ScratchWaveSizeGranularityShiftNv31;
        m_scratchWaveSizeGranularity      = ScratchWaveSizeGranularityNv31;
    }
    else
#endif
    {
        m_scratchWaveSizeGranularityShift = ScratchWaveSizeGranularityShift;
        m_scratchWaveSizeGranularity      = ScratchWaveSizeGranularity;
    }

    m_numTotalCus = chipProps.gfx9.numShaderEngines *
                    chipProps.gfx9.numShaderArrays  *
                    chipProps.gfx9.numCuPerSh;

    // The max we expect is one scratch wave on every wave slot in every CU.
    m_numMaxWaves = (chipProps.gfx9.numWavesPerSimd * chipProps.gfx9.numSimdPerCu * m_numTotalCus);

    ShaderRingSrd srdTableIndex = ShaderRingSrd::ScratchGraphics;

    if (shaderType == ShaderCompute)
    {
        srdTableIndex = ShaderRingSrd::ScratchCompute;

        // We must allow for at least as many waves as there are in the largest threadgroup.
        const uint32 maxWaves = chipProps.gfxip.maxThreadGroupSize / chipProps.gfx9.minWavefrontSize;
        m_numMaxWaves         = Max<size_t>(m_numMaxWaves, maxWaves);
    }

    // The hardware can only support a limited number of scratch waves per CU so make sure we don't exceed that number.
    m_numMaxWaves = Min<size_t>(m_numMaxWaves, (MaxScratchWavesPerCu * m_numTotalCus));
    PAL_ASSERT(m_numMaxWaves <= 0xFFF); // Max bits allowed in reg field, should never hit this.

    BufferSrd*const   pGenericSrd = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    m_pDevice->InitBufferSrd(pGenericSrd, 0, 0);
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        auto*const  pSrd = &pGenericSrd->gfx9;

        pSrd->word1.bits.SWIZZLE_ENABLE  = 1;
        pSrd->word3.bits.INDEX_STRIDE    = BUF_INDEX_STRIDE_64B;
        pSrd->word3.bits.ADD_TID_ENABLE  = 1;
    }
    else if (IsGfx10(m_gfxLevel))
    {
        auto*const  pSrd = &pGenericSrd->gfx10;

        pSrd->gfx10.swizzle_enable = 1;
        pSrd->index_stride         = BUF_INDEX_STRIDE_64B;
        pSrd->add_tid_enable       = 1;
    }
#if PAL_BUILD_GFX11
    else if (IsGfx11(m_gfxLevel))
    {
        auto*const  pSrd = &pGenericSrd->gfx10;

        pSrd->gfx11.swizzle_enable = 1;
        pSrd->index_stride         = BUF_INDEX_STRIDE_64B;
        pSrd->add_tid_enable       = 1;
    }
#endif
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    AdjustRingDataFormat(chipProps, pGenericSrd);
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
        size_t allocSize   = static_cast<size_t>(m_allocSize);
        size_t numMaxWaves = m_numMaxWaves;

#if PAL_BUILD_GFX11
        // On Gfx11+, the scratch ring registers describe the number of waves per SE rather than per-chip,
        // as with previous architectures.
        if (IsGfx11(chipProps.gfxLevel))
        {
            allocSize   /= chipProps.gfx9.numShaderEngines;
            numMaxWaves /= chipProps.gfx9.numShaderEngines;
        }
#endif
        numWaves = Min(static_cast<size_t>(allocSize) / (waveSize * sizeof(uint32)), numMaxWaves);
    }

    // Max bits allowed in reg field, should never hit this.
    PAL_ASSERT(numWaves <= 0xFFF);

    return numWaves;
}

// =====================================================================================================================
// Calculates the the wave size for the PM4 packet which identifies the particular shader type of this ring. Returns the
// amount of space used by each wave in DWORDs.
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
    // Clamp scratch wave size to be <= 2M - 256 per register spec requirement. This will ensure that the calculation
    // of number of waves below will not exceed what SPI can actually generate.
    const size_t MaxWaveSize        = ((1 << 21) - 256);
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

    return Max(Min(MaxWaveSize, adjScratchWaveSize), minWaveSize);
}

// =====================================================================================================================
// Overrides the base class' method for computing the scratch buffer size.
gpusize ScratchRing::ComputeAllocationSize() const
{
    const Pal::Device*         pParent         = m_pDevice->Parent();
    const GpuChipProperties&   chipProps       = pParent->ChipProperties();
    const GpuMemoryProperties& memProps        = pParent->MemoryProperties();
    const PalSettings&         settings        = pParent->Settings();
    const PalPublicSettings*   pPublicSettings = pParent->GetPublicSettings();

    // Compute the adjusted scratch size required by each wave.
    const size_t waveSize = AdjustScratchWaveSize(m_itemSizeMax * chipProps.gfx9.minWavefrontSize);

    // The ideal size to allocate for this Ring is: threadsPerWavefront * maxWaves * itemSize DWORDs.
    // We clamp this allocation to a maximum size to prevent the driver from using an unreasonable amount of scratch.
    const gpusize totalLocalMemSize =
        pParent->HeapLogicalSize(GpuHeapLocal) + pParent->HeapLogicalSize(GpuHeapInvisible);
    const gpusize maxScaledSize     = (pPublicSettings->maxScratchRingSizeScalePct * totalLocalMemSize) / 100;
    const gpusize maxSize           = Max(pPublicSettings->maxScratchRingSizeBaseline, maxScaledSize);
    const gpusize allocationSize    = static_cast<gpusize>(m_numMaxWaves) * waveSize * sizeof(uint32);

    return Min(allocationSize, maxSize);
}

// =====================================================================================================================
void ScratchRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const ShaderRingSrd srdTableIndex = (m_shaderType == ShaderCompute) ? ShaderRingSrd::ScratchCompute :
                                                                          ShaderRingSrd::ScratchGraphics;
    const gpusize       addr          = m_ringMem.GpuVirtAddr();
    BufferSrd*const     pSrd          = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    m_pDevice->SetBaseAddress(pSrd, addr);
    m_pDevice->SetNumRecords(pSrd, MemorySizeBytes());
}

// =====================================================================================================================
GsVsRing::GsVsRing(
    Device*    pDevice,
    BufferSrd* pSrdTable,
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::GsVs)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    // The factor of two is to double-buffer this ring to give the HW some "breathing room" since since space in this
    // ring is not deallocated until the copy shader completes. There is 1 VGT per SE.
    m_numMaxWaves = (chipProps.gfx9.maxGsWavesPerVgt * chipProps.gfx9.numShaderEngines * 2);

    BufferSrd*const pGenericSrdWr = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsWrite0)];
    BufferSrd*const pGenericSrdRd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsRead)];

    // Set-up static SRD fields for Write:
    for (size_t idx = 0; idx < WriteSrds; ++idx)
    {
        auto*const  pBufferSrdWr = &pGenericSrdWr[idx];

        pDevice->InitBufferSrd(pBufferSrdWr, 0, 0);
        pDevice->SetNumRecords(pBufferSrdWr, NumRecordsWrite);

        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            auto*const  pSrdWr = &pBufferSrdWr->gfx9;

            pSrdWr->word1.bits.SWIZZLE_ENABLE = 1;
            pSrdWr->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_INVALID;
            pSrdWr->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
            pSrdWr->word3.bits.INDEX_STRIDE   = BUF_INDEX_STRIDE_16B;
            pSrdWr->word3.bits.ADD_TID_ENABLE = 1;
        }
        else if (IsGfx10(m_gfxLevel))
        {
            auto*const  pSrdWr = &pBufferSrdWr->gfx10;

            pSrdWr->gfx10.swizzle_enable = 1;
            pSrdWr->index_stride         = BUF_INDEX_STRIDE_16B;
            pSrdWr->add_tid_enable       = 1;
        }
#if PAL_BUILD_GFX11
        else if (IsGfx11(m_gfxLevel))
        {
            auto*const  pSrdWr = &pBufferSrdWr->gfx10;

            pSrdWr->gfx11.swizzle_enable = 1;
            pSrdWr->index_stride         = BUF_INDEX_STRIDE_16B;
            pSrdWr->add_tid_enable       = 1;
        }
#endif
        else
        {
            PAL_ASSERT_ALWAYS();
        }

        AdjustRingDataFormat(chipProps, pGenericSrdWr + idx);
    }

    // Set-up static SRD fields for Read:
    pDevice->InitBufferSrd(pGenericSrdRd, 0, 0);

    AdjustRingDataFormat(chipProps, pGenericSrdRd);
}

// =====================================================================================================================
void GsVsRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrdRd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsRead)];

    m_pDevice->SetBaseAddress(pSrdRd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrdRd, MemorySizeBytes());

    for (size_t idx = 0; idx < WriteSrds; ++idx)
    {
        BufferSrd*const pSrdWr = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsWrite0) + idx];

        m_pDevice->SetBaseAddress(pSrdWr, gpuVirtAddr);

        // All four WriteSrds are programmed to the same base address and a stride of zero.
        // These SRDs are patched by the geometry shader with values from a geometry constant buffer for
        // accurate rendering.
        if (m_gfxLevel == GfxIpLevel::GfxIp9)
        {
            pSrdWr->gfx9.word1.bits.STRIDE = 0;
        }
        else if (IsGfx10Plus(m_gfxLevel))
        {
            pSrdWr->gfx10.stride = 0;
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
TessFactorBuffer::TessFactorBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable,
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::TfBuffer)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    BufferSrd*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TessFactorBuffer)];

    // Set-up static SRD fields:
    pDevice->InitBufferSrd(pGenericSrd, 0, 0);

    AdjustRingDataFormat(chipProps, pGenericSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the TF buffer size, since the size of the TF buffer is fixed and
// depends on the number of shader engines present. Returns the allocation size, in bytes.
gpusize TessFactorBuffer::ComputeAllocationSize() const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx9PalSettings&   settings  = m_pDevice->Settings();

    return (settings.tessFactorBufferSizePerSe * chipProps.gfx9.numShaderEngines * sizeof(uint32));
}

// =====================================================================================================================
void TessFactorBuffer::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TessFactorBuffer)];
    m_pDevice->SetBaseAddress(pSrd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrd, m_allocSize);
}

// =====================================================================================================================
uint32 TessFactorBuffer::TfRingSize() const
{
    uint32 tfRingSize = static_cast<uint32>(MemorySizeDwords());

#if PAL_BUILD_GFX11
    if (IsGfx11(*m_pDevice->Parent()))
    {
        const uint32 numShaderEngines = m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;
        tfRingSize /= numShaderEngines;
    }
#endif

    return tfRingSize;
}

// =====================================================================================================================
OffchipLdsBuffer::OffchipLdsBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable, // Pointer to our parent ring-set's SRD table
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::OffChipLds)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    BufferSrd*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::OffChipLdsBuffer)];

    pDevice->InitBufferSrd(pGenericSrd, 0, 0);

    AdjustRingDataFormat(chipProps, pGenericSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the offchip LDS buffer size, since the size of the offchip LDS buffer
// depends on the number of offchip LDS buffers available to the chip. Returns the allocation size, in bytes.
gpusize OffchipLdsBuffer::ComputeAllocationSize() const
{
    // Determine the LDS buffer size in DWORD's based on settings.
    const gpusize offchipLdsBufferSizeBytes = m_pDevice->Parent()->ChipProperties().gfxip.offChipTessBufferSize;

    // Our maximum item size represents how many offchip LDS buffers we need space for in total.
    return (offchipLdsBufferSizeBytes * m_itemSizeMax);
}

// =====================================================================================================================
void OffchipLdsBuffer::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::OffChipLdsBuffer)];
    m_pDevice->SetBaseAddress(pSrd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrd, m_allocSize);
}

// =====================================================================================================================
uint32 OffchipLdsBuffer::OffchipBuffering() const
{
    uint32 offchipBuffering = static_cast<uint32>(m_itemSizeMax);

#if PAL_BUILD_GFX11
    if (IsGfx11(*m_pDevice->Parent()))
    {
        const uint32 numShaderEngines = m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;
        offchipBuffering /= numShaderEngines;
    }
#endif

    // OFFCHIP_BUFFERING setting is biased by one (i.e., 0=1, 511=512, etc.).
    return offchipBuffering - 1;
}

// =====================================================================================================================
SamplePosBuffer::SamplePosBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable, // Pointer to our parent ring-set's SRD table
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::SamplePos)
{
    constexpr uint32 SamplePosBufStride = sizeof(float) * 4;

    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    BufferSrd*const pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::SamplePosBuffer)];

    // Set-up static SRD fields:
    pDevice->InitBufferSrd(pGenericSrd, 0, SamplePosBufStride);
    AdjustRingDataFormat(chipProps, pGenericSrd);
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

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::SamplePosBuffer)];
    m_pDevice->SetBaseAddress(pSrd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrd, m_allocSize);
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
MeshScratchRing::MeshScratchRing(
    Device*       pDevice,
    BufferSrd*    pSrdTable,
    bool          isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::MeshScratch),
    m_maxThreadgroupsPerChip(1 << CountSetBits(VGT_GS_MAX_WAVE_ID__MAX_WAVE_ID_MASK))
{
    BufferSrd*const   pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::MeshScratch)];

    m_pDevice->InitBufferSrd(pGenericSrd, 0, 0);
    AdjustRingDataFormat(m_pDevice->Parent()->ChipProperties(), pGenericSrd);
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

// =====================================================================================================================
PayloadDataRing::PayloadDataRing(
    Device*    pDevice,
    BufferSrd* pSrdTable,
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::PayloadData),
    m_maxNumEntries(Pow2Pad(m_pDevice->Settings().numTsMsDrawEntriesPerSe *
                            pDevice->Parent()->ChipProperties().gfx9.numShaderEngines))
{
    const GpuChipProperties& chipProps   = m_pDevice->Parent()->ChipProperties();
    BufferSrd*const          pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::PayloadDataRing)];
    m_pDevice->InitBufferSrd(pGenericSrd, 0, PayloadDataEntrySize);
    AdjustRingDataFormat(chipProps, pGenericSrd);
}

// =====================================================================================================================
void PayloadDataRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::PayloadDataRing)];
    m_pDevice->SetBaseAddress(pSrd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrd, m_allocSize);
}

// =====================================================================================================================
TaskMeshCtrlDrawRing::TaskMeshCtrlDrawRing(
    Device*    pDevice,
    BufferSrd* pSrdTable,
    bool       isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::TaskMeshCtrlDrawRing),
    m_drawRingEntries(Pow2Pad(m_pDevice->Settings().numTsMsDrawEntriesPerSe *
                              pDevice->Parent()->ChipProperties().gfx9.numShaderEngines)),
    m_drawRingTotalBytes(m_drawRingEntries * DrawDataEntrySize)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    BufferSrd*const          pDrawData = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::DrawDataRing)];
    m_pDevice->InitBufferSrd(pDrawData, 0, DrawDataEntrySize);
    AdjustRingDataFormat(chipProps, pDrawData);

    BufferSrd*const          pTaskMeshCt = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TaskMeshControl)];
    m_pDevice->InitBufferSrd(pTaskMeshCt, 0, 0);
    AdjustRingDataFormat(chipProps, pTaskMeshCt);
}

// =====================================================================================================================
void TaskMeshCtrlDrawRing::InitializeControlBufferAndDrawRingBuffer()
{
    ControlBufferLayout controlBuffer = {};

    constexpr uint32 AlignmentBytes = 64;

    // The constant offset must be > 64 bytes and it must be power of two.
    static_assert(Util::IsPowerOfTwo(OffsetOfControlDrawRing) && (OffsetOfControlDrawRing > AlignmentBytes),
                  "The offset between control buffer and draw ring buffer must be power of two and aligned to 64!");

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

    BufferSrd*const          pGenericSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::DrawDataRing)];
    m_pDevice->SetBaseAddress(pGenericSrd, GetDrawRingVirtAddr());
    m_pDevice->SetNumRecords(pGenericSrd, m_drawRingTotalBytes);
}

#if PAL_BUILD_GFX11
// =====================================================================================================================
VertexAttributeRing::VertexAttributeRing(
    Device*     pDevice,
    BufferSrd*  pSrdTable,
    bool        isTmz)
    :
    ShaderRing(pDevice, pSrdTable, isTmz, ShaderRingType::VertexAttributes)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::VertexAttributes)];

    // Set-up static SRD fields:
    pDevice->InitBufferSrd(pSrd, 0, Stride);

    AdjustRingDataFormat(chipProps, pSrd);
    pSrd->gfx10.index_stride          = BUF_INDEX_STRIDE_32B;
    pSrd->gfx10.gfx104Plus.format     = BUF_FMT_32_32_32_32_FLOAT__GFX104PLUS;
    pSrd->gfx10.gfx11.swizzle_enable  = 3;
}

// =====================================================================================================================
Result VertexAttributeRing::AllocateVideoMemory(
    gpusize             memorySizeBytes,
    ShaderRingMemory*   pDeferredMem)
{
    m_pDevice->AllocateVertexAttributesMem(m_tmzEnabled);
    const BoundGpuMemory& vertexAttributesMem = m_pDevice->VertexAttributesMem(m_tmzEnabled);

    m_ringMem.Update(vertexAttributesMem.Memory(), vertexAttributesMem.Offset());

    return Result::Success;
}

// =====================================================================================================================
// Overrides the base class's function for computing the ring size.
// Returns the allocation size in bytes.
gpusize VertexAttributeRing::ComputeAllocationSize() const
{
    const uint32 numSes = m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;

    gpusize sizeBytes = m_itemSizeMax * numSes;

    // The size of this allocation must be aligned per SE
    PAL_ASSERT((sizeBytes / numSes) % Gfx11VertexAttributeRingAlignmentBytes == 0);
    PAL_ASSERT(sizeBytes < Gfx11VertexAttributeRingMaxSizeBytes);

    return sizeBytes;
}

// =====================================================================================================================
void VertexAttributeRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::VertexAttributes)];
    m_pDevice->SetBaseAddress(pSrd, gpuVirtAddr);
    m_pDevice->SetNumRecords(pSrd, Device::CalcNumRecords(static_cast<size_t>(m_allocSize), Stride));
}
#endif

} // Gfx9
} // Pal
