/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRing.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
// On GFXIP 9 hardware, buffer SRD's which set the ADD_TID_ENABLE bit in word3 changes the meaning of the DATA_FORMAT
// field to stride bits [17:14] used for scratch offset boundary checks instead of the format.
static PAL_INLINE void AdjustRingDataFormat(
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
    Device*    pDevice,
    BufferSrd* pSrdTable)
    :
    m_pDevice(pDevice),
    m_pSrdTable(pSrdTable),
    m_allocSize(0),
    m_numMaxWaves(0),
    m_itemSizeMax(0),
    m_gfxLevel(pDevice->Parent()->ChipProperties().gfxLevel)
{
}

// =====================================================================================================================
ShaderRing::~ShaderRing()
{
    if (m_ringMem.IsBound())
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
    return (chipProps.gfx9.wavefrontSize * m_numMaxWaves * m_itemSizeMax * sizeof(uint32));
}

// =====================================================================================================================
Result ShaderRing::AllocateVideoMemory(
    gpusize        memorySizeBytes,
    ShaderRingType ringType)
{
    InternalMemMgr*const pMemMgr = m_pDevice->Parent()->MemMgr();

    if (m_ringMem.IsBound())
    {
        // Need to release any preexisting memory allocation.
        pMemMgr->FreeGpuMem(m_ringMem.Memory(), m_ringMem.Offset());
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

    if (ringType == ShaderRingType::SamplePos)
    {
        createInfo.heaps[0]  = GpuHeapLocal;
        createInfo.heaps[1]  = GpuHeapGartUswc;
        createInfo.heapCount = 2;
    }
    else
    {
        createInfo.heaps[0]  = GpuHeapInvisible;
        createInfo.heaps[1]  = GpuHeapLocal;
        createInfo.heaps[2]  = GpuHeapGartUswc;
        createInfo.heapCount = 3;
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
    size_t         itemSize, // Item size of the Ring to validate against (in DWORDs)
    ShaderRingType ringType)
{
    Result result = Result::Success;

    // Only need to validate if the new item size is larger than the largest we've validated thus far.
    if (itemSize > m_itemSizeMax)
    {
        m_itemSizeMax = itemSize;
        const gpusize sizeNeeded = ComputeAllocationSize();

        // Attempt to allocate the video memory for this Ring.
        result = AllocateVideoMemory(sizeNeeded, ringType);
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
    Pm4ShaderType shaderType)
    :
    ShaderRing(pDevice, pSrdTable),
    m_shaderType(shaderType),
    m_numTotalCus(0)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const auto pPalSettings  = m_pDevice->Parent()->GetPublicSettings();

    m_numTotalCus = chipProps.gfx9.numShaderEngines *
                    chipProps.gfx9.numShaderArrays  *
                    chipProps.gfx9.numCuPerSh;

    m_numMaxWaves = (pPalSettings->numScratchWavesPerCu * m_numTotalCus);

    ShaderRingSrd srdTableIndex = ShaderRingSrd::ScratchGraphics;

    if (shaderType == ShaderCompute)
    {
        srdTableIndex = ShaderRingSrd::ScratchCompute;

        // We must allow for at least as many waves as there are in the largest threadgroup.
        m_numMaxWaves = Max<size_t>(m_numMaxWaves, (chipProps.gfxip.maxThreadGroupSize / chipProps.gfx9.wavefrontSize));
    }

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
    else
    {
        PAL_ASSERT_ALWAYS();
    }

    AdjustRingDataFormat(chipProps, pGenericSrd);
}

// =====================================================================================================================
// Calculates the the wave size for the PM4 packet which identifies the particular shader type of this ring. Returns the
// amount of space used by each wave in DWORDs.
size_t ScratchRing::CalculateWaveSize() const
{
    const     GpuChipProperties& chipProps                = m_pDevice->Parent()->ChipProperties();
    constexpr uint32             WaveSizeGranularityShift = 8;

    return (m_itemSizeMax * chipProps.gfx9.wavefrontSize) >> WaveSizeGranularityShift;
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
    BufferSrd* pSrdTable)
    :
    ShaderRing(pDevice, pSrdTable)
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
        else
        {
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
TessFactorBuffer::TessFactorBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable)
    :
    ShaderRing(pDevice, pSrdTable)
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

    // The Tahiti register spec recommends a TF buffer size of 0x2000 DWORDs per shader engine, but discussions
    // indicate that 0x1000 DWORDs per SE is preferable.
    gpusize allocSize = (settings.tessFactorBufferSizePerSe * chipProps.gfx9.numShaderEngines * sizeof(uint32));

    return allocSize;
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
OffchipLdsBuffer::OffchipLdsBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable) // Pointer to our parent ring-set's SRD table
    :
    ShaderRing(pDevice, pSrdTable)
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
    gpusize offchipLdsBufferSizeDwords = 0;
    switch (m_pDevice->Settings().offchipLdsBufferSize)
    {
    case OffchipLdsBufferSize1024:
        offchipLdsBufferSizeDwords = 1024;
        break;
    case OffchipLdsBufferSize2048:
        offchipLdsBufferSizeDwords = 2048;
        break;
    case OffchipLdsBufferSize4096:
        offchipLdsBufferSizeDwords = 4096;
        break;
    case OffchipLdsBufferSize8192:
        offchipLdsBufferSizeDwords = 8192;
        break;
    default:
        PAL_NEVER_CALLED();
        break;
    }

    // Our maximum item size represents how many offchip LDS buffers we need space for in total.
    return (offchipLdsBufferSizeDwords * sizeof(uint32) * m_itemSizeMax);
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
SamplePosBuffer::SamplePosBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable) // Pointer to our parent ring-set's SRD table
    :
    ShaderRing(pDevice, pSrdTable)
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

} // Gfx9
} // Pal
