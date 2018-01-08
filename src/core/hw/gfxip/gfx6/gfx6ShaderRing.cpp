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
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRing.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
{

// =====================================================================================================================
// On GFXIP 8.x hardware, buffer SRD's which set the ADD_TID_ENABLE bit in word3 changes the meaning of the DATA_FORMAT
// field to stride bits [17:14] used for scratch offset boundary checks instead of the format.
static PAL_INLINE void AdjustRingDataFormat(
    const GpuChipProperties& chipProps,
    BufferSrd*               pSrd)
{
    if ((chipProps.gfxLevel >= GfxIpLevel::GfxIp8) && (pSrd->word3.bits.ADD_TID_ENABLE != 0))
    {
        pSrd->word3.bits.DATA_FORMAT = static_cast<BUF_DATA_FORMAT>(0); // Sets the extended stride to zero.
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
    m_itemSizeMax(0)
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
    return (chipProps.gfx6.wavefrontSize * m_numMaxWaves * m_itemSizeMax * sizeof(uint32));
}

// =====================================================================================================================
Result ShaderRing::AllocateVideoMemory(
    gpusize memorySizeBytes,
    bool    cpuVisible)
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

    GpuMemoryCreateInfo createInfo = { };
    createInfo.size      = memorySizeBytes;
    createInfo.alignment = ShaderRingAlignment;
    createInfo.priority  = GpuMemPriority::Normal;
    if (cpuVisible)
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

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident = 1;

    GpuMemory* pGpuMemory = nullptr;
    gpusize    memOffset  = 0;

    // Allocate video memory for this Ring.
    Result result = pMemMgr->AllocateGpuMem(createInfo, internalInfo, 0, &pGpuMemory, &memOffset);
    if (result == Result::Success)
    {
        m_ringMem.Update(pGpuMemory, memOffset);
    }

    return result;
}

// =====================================================================================================================
// Performs submit-time validation on this shader Ring so that any dirty state can be updated.
Result ShaderRing::Validate(
    size_t                      itemSize,              // Item size of the Ring to validate against (in DWORDs)
    ShaderRingType              ringType)
{
    Result result = Result::Success;

    // Only need to validate if the new item size is larger than the largest we've validated thus far.
    if (itemSize > m_itemSizeMax)
    {
        m_itemSizeMax = itemSize;
        const gpusize sizeNeeded = ComputeAllocationSize();

        // Attempt to allocate the video memory for this Ring.
        result = AllocateVideoMemory(sizeNeeded, (ringType == ShaderRingType::SamplePos));

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
    PM4ShaderType shaderType)
    :
    ShaderRing(pDevice, pSrdTable),
    m_shaderType(shaderType),
    m_numTotalCus(0)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const auto pPalSettings = m_pDevice->Parent()->GetPublicSettings();

    m_numTotalCus = chipProps.gfx6.numShaderEngines *
                    chipProps.gfx6.numShaderArrays  *
                    chipProps.gfx6.numCuPerSh;

    m_numMaxWaves = (pPalSettings->numScratchWavesPerCu * m_numTotalCus);

    ShaderRingSrd srdTableIndex = ShaderRingSrd::ScratchGraphics;

    if (shaderType == ShaderCompute)
    {
        srdTableIndex = ShaderRingSrd::ScratchCompute;

        // We must allow for at least as many waves as there are in the largest threadgroup.
        m_numMaxWaves = Max<size_t>(m_numMaxWaves, (chipProps.gfxip.maxThreadGroupSize / chipProps.gfx6.wavefrontSize));
    }

    m_numMaxWaves = Min<size_t>(m_numMaxWaves, (MaxScratchWavesPerCu * m_numTotalCus));
    PAL_ASSERT(m_numMaxWaves <= 0xFFF); // Max bits allowed in reg field, should never hit this.

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    pSrd->word1.bits.STRIDE          = 0;
    pSrd->word1.bits.SWIZZLE_ENABLE  = 1;
    pSrd->word1.bits.CACHE_SWIZZLE   = 0;

    pSrd->word3.bits.DST_SEL_X       = SQ_SEL_X;
    pSrd->word3.bits.DST_SEL_Y       = SQ_SEL_Y;
    pSrd->word3.bits.DST_SEL_Z       = SQ_SEL_Z;
    pSrd->word3.bits.DST_SEL_W       = SQ_SEL_W;
    pSrd->word3.bits.NUM_FORMAT      = BUF_NUM_FORMAT_FLOAT;
    pSrd->word3.bits.ELEMENT_SIZE    = BUF_ELEMENT_SIZE_4B;
    pSrd->word3.bits.INDEX_STRIDE    = BUF_INDEX_STRIDE_64B;
    pSrd->word3.bits.ADD_TID_ENABLE  = 1;
    pSrd->word3.bits.TYPE            = SQ_RSRC_BUF;
    pSrd->word3.bits.HASH_ENABLE     = 0;
    pSrd->word3.bits.DATA_FORMAT     = BUF_DATA_FORMAT_32;

    AdjustRingDataFormat(chipProps, pSrd);
}

// =====================================================================================================================
// Calculates the the wave size for the PM4 packet which identifies the particular shader type of this ring. Returns the
// amount of space used by each wave in DWORDs.
size_t ScratchRing::CalculateWaveSize() const
{
    const     GpuChipProperties& chipProps                = m_pDevice->Parent()->ChipProperties();
    constexpr uint32             WaveSizeGranularityShift = 8;

    return (m_itemSizeMax * chipProps.gfx6.wavefrontSize) >> WaveSizeGranularityShift;
}

// =====================================================================================================================
void ScratchRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const ShaderRingSrd srdTableIndex = (m_shaderType == ShaderCompute) ? ShaderRingSrd::ScratchCompute :
                                                                          ShaderRingSrd::ScratchGraphics;

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(srdTableIndex)];

    const gpusize addr = m_ringMem.GpuVirtAddr();

    pSrd->word0.bits.BASE_ADDRESS    = LowPart(addr);
    pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(addr);
    pSrd->word2.bits.NUM_RECORDS     = MemorySizeBytes();
}

// =====================================================================================================================
EsGsRing::EsGsRing(
    Device*    pDevice,
    BufferSrd* pSrdTable)
    :
    ShaderRing(pDevice, pSrdTable)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    // The ES runs N times as many threads than the GS based on the GS input topology. Get an
    // approximation for N from a setting. The factor of two is to double-buffer this ring to
    // give the HW some "breathing room" since space in this ring is not deallocated until the
    // copy shader completes. There is 1 VGT per SE.
    const size_t esGsRatio = (2 * m_pDevice->Settings().esGsRatio);
    m_numMaxWaves = (chipProps.gfx6.maxGsWavesPerVgt * chipProps.gfx6.numShaderEngines * esGsRatio);

    BufferSrd*const pSrdWr = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::EsGsWrite)];
    BufferSrd*const pSrdRd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::EsGsRead)];

    // Set-up static SRD fields for Write:
    pSrdWr->word1.bits.STRIDE         = 0;
    pSrdWr->word1.bits.SWIZZLE_ENABLE = 1;
    pSrdWr->word1.bits.CACHE_SWIZZLE  = 0;

    pSrdWr->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrdWr->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrdWr->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrdWr->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrdWr->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrdWr->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrdWr->word3.bits.ELEMENT_SIZE   = BUF_ELEMENT_SIZE_4B;
    pSrdWr->word3.bits.INDEX_STRIDE   = BUF_INDEX_STRIDE_64B;
    pSrdWr->word3.bits.ADD_TID_ENABLE = 1;
    pSrdWr->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrdWr->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrdWr);

    // Set-up static SRD fields for Read:
    pSrdRd->word1.bits.STRIDE         = 0;
    pSrdRd->word1.bits.SWIZZLE_ENABLE = 0;
    pSrdRd->word1.bits.CACHE_SWIZZLE  = 0;

    pSrdRd->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrdRd->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrdRd->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrdRd->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrdRd->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrdRd->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrdRd->word3.bits.ADD_TID_ENABLE = 0;
    pSrdRd->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrdRd->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrdRd);
}

// =====================================================================================================================
void EsGsRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::EsGsWrite)];

    for (size_t idx = 0; idx < TotalSrds; ++idx)
    {
        pSrd[idx].word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
        pSrd[idx].word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
        pSrd[idx].word2.bits.NUM_RECORDS     = MemorySizeBytes();
    }
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
    m_numMaxWaves = (chipProps.gfx6.maxGsWavesPerVgt * chipProps.gfx6.numShaderEngines * 2);

    BufferSrd*const pSrdWr = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsWrite0)];
    BufferSrd*const pSrdRd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsRead)];

    // Set-up static SRD fields for Write:
    for (size_t idx = 0; idx < WriteSrds; ++idx)
    {
        pSrdWr[idx].word1.bits.STRIDE         = 0;
        pSrdWr[idx].word1.bits.SWIZZLE_ENABLE = 1;
        pSrdWr[idx].word1.bits.CACHE_SWIZZLE  = 0;

        pSrdWr[idx].word2.bits.NUM_RECORDS = NumRecordsWrite;
        pSrdWr[idx].word3.bits.DST_SEL_X   = SQ_SEL_X;
        pSrdWr[idx].word3.bits.DST_SEL_Y   = SQ_SEL_Y;
        pSrdWr[idx].word3.bits.DST_SEL_Z   = SQ_SEL_Z;
        pSrdWr[idx].word3.bits.DST_SEL_W   = SQ_SEL_W;
        pSrdWr[idx].word3.bits.DATA_FORMAT = BUF_DATA_FORMAT_32;

        pSrdWr[idx].word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
        pSrdWr[idx].word3.bits.ELEMENT_SIZE   = BUF_ELEMENT_SIZE_4B;
        pSrdWr[idx].word3.bits.INDEX_STRIDE   = BUF_INDEX_STRIDE_16B;
        pSrdWr[idx].word3.bits.ADD_TID_ENABLE = 1;
        pSrdWr[idx].word3.bits.TYPE           = SQ_RSRC_BUF;
        pSrdWr[idx].word3.bits.HASH_ENABLE    = 0;

        AdjustRingDataFormat(chipProps, pSrdWr + idx);
    }

    // Set-up static SRD fields for Read:
    pSrdRd->word1.bits.STRIDE         = 0;
    pSrdRd->word1.bits.SWIZZLE_ENABLE = 0;
    pSrdRd->word1.bits.CACHE_SWIZZLE  = 0;

    pSrdRd->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrdRd->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrdRd->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrdRd->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrdRd->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrdRd->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrdRd->word3.bits.ADD_TID_ENABLE = 0;
    pSrdRd->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrdRd->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrdRd);
}

// =====================================================================================================================
void GsVsRing::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrdRd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsRead)];

    pSrdRd->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
    pSrdRd->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
    pSrdRd->word2.bits.NUM_RECORDS     = MemorySizeBytes();

    for (size_t idx = 0; idx < WriteSrds; ++idx)
    {
        BufferSrd*const pSrdWr = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::GsVsWrite0) + idx];

        // All four WriteSrds are programmed to the same base address and a stride of zero.
        // These SRDs are patched by the geometry shader with values from a geometry constant buffer for
        // accurate rendering.
        pSrdWr->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
        pSrdWr->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
        pSrdWr->word1.bits.STRIDE          = 0;
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

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TessFactorBuffer)];

    // Set-up static SRD fields:
    pSrd->word1.bits.STRIDE         = 0;
    pSrd->word1.bits.SWIZZLE_ENABLE = 0;
    pSrd->word1.bits.CACHE_SWIZZLE  = 0;

    pSrd->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrd->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrd->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrd->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrd->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrd->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrd->word3.bits.ADD_TID_ENABLE = 0;
    pSrd->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrd->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the TF buffer size, since the size of the TF buffer is fixed and
// depends on the number of shader engines present. Returns the allocation size, in bytes.
gpusize TessFactorBuffer::ComputeAllocationSize() const
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();
    const Gfx6PalSettings&   settings  = m_pDevice->Settings();

    // The Tahiti register spec recommends a TF buffer size of 0x2000 DWORDs per shader engine, but discussions
    // indicate that 0x1000 DWORDs per SE is preferable.
    return (settings.tessFactorBufferSizePerSe * chipProps.gfx6.numShaderEngines * sizeof(uint32));
}

// =====================================================================================================================
void TessFactorBuffer::UpdateSrds() const
{
    PAL_ASSERT(m_ringMem.IsBound());

    const gpusize gpuVirtAddr = m_ringMem.GpuVirtAddr();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::TessFactorBuffer)];

    pSrd->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
    pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
    pSrd->word2.bits.NUM_RECORDS     = m_allocSize;
}

// =====================================================================================================================
OffchipLdsBuffer::OffchipLdsBuffer(
    Device*    pDevice,
    BufferSrd* pSrdTable) // Pointer to our parent ring-set's SRD table
    :
    ShaderRing(pDevice, pSrdTable)
{
    const GpuChipProperties& chipProps = m_pDevice->Parent()->ChipProperties();

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::OffChipLdsBuffer)];

    // Set-up static SRD fields:
    pSrd->word1.bits.STRIDE         = 0;
    pSrd->word1.bits.SWIZZLE_ENABLE = 0;
    pSrd->word1.bits.CACHE_SWIZZLE  = 0;

    pSrd->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrd->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrd->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrd->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrd->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrd->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrd->word3.bits.ADD_TID_ENABLE = 0;
    pSrd->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrd->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrd);
}

// =====================================================================================================================
// Overrides the base class' method for computing the offchip LDS buffer size, since the size of the offchip LDS buffer
// depends on the number of offchip LDS buffers available to the chip. Returns the allocation size, in bytes.
gpusize OffchipLdsBuffer::ComputeAllocationSize() const
{
    // Determine the LDS buffer size in DWORD's based on settings.
    gpusize offchipLdsBufferSizeDwords = 0;
    switch (m_pDevice->Settings().gfx7OffchipLdsBufferSize)
    {
    case Gfx7OffchipLdsBufferSize1024:
        offchipLdsBufferSizeDwords = 1024;
        break;
    case Gfx7OffchipLdsBufferSize2048:
        offchipLdsBufferSizeDwords = 2048;
        break;
    case Gfx7OffchipLdsBufferSize4096:
        offchipLdsBufferSizeDwords = 4096;
        break;
    default:
        PAL_NEVER_CALLED();
    case Gfx7OffchipLdsBufferSize8192:
        offchipLdsBufferSizeDwords = 8192;
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

    pSrd->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
    pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
    pSrd->word2.bits.NUM_RECORDS     = m_allocSize;
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

    BufferSrd*const pSrd = &m_pSrdTable[static_cast<size_t>(ShaderRingSrd::SamplePosBuffer)];

    // Set-up static SRD fields:
    pSrd->word1.bits.STRIDE = SamplePosBufStride;
    pSrd->word1.bits.SWIZZLE_ENABLE = 0;
    pSrd->word1.bits.CACHE_SWIZZLE = 0;

    pSrd->word3.bits.DST_SEL_X      = SQ_SEL_X;
    pSrd->word3.bits.DST_SEL_Y      = SQ_SEL_Y;
    pSrd->word3.bits.DST_SEL_Z      = SQ_SEL_Z;
    pSrd->word3.bits.DST_SEL_W      = SQ_SEL_W;
    pSrd->word3.bits.NUM_FORMAT     = BUF_NUM_FORMAT_FLOAT;
    pSrd->word3.bits.DATA_FORMAT    = BUF_DATA_FORMAT_32;
    pSrd->word3.bits.ADD_TID_ENABLE = 0;
    pSrd->word3.bits.TYPE           = SQ_RSRC_BUF;
    pSrd->word3.bits.HASH_ENABLE    = 0;

    AdjustRingDataFormat(chipProps, pSrd);
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

    pSrd->word0.bits.BASE_ADDRESS    = LowPart(gpuVirtAddr);
    pSrd->word1.bits.BASE_ADDRESS_HI = HighPart(gpuVirtAddr);
    pSrd->word2.bits.NUM_RECORDS     = m_allocSize;
}

// =====================================================================================================================
void SamplePosBuffer::UploadSamplePatternPalette(
    const SamplePatternPalette& samplePatternPalette)
{
    // Update sample pattern palette buffer when m_ringMem has video memory bound, which also means
    // IDevice::SetSamplePatternPalette is called by client, and CPU visible video memory is allocated.
    if (m_ringMem.IsBound())
    {
        void* pData   = nullptr;
        Result result = m_ringMem.Map(&pData);
        if (result == Result::Success)
        {
            memcpy(pData, samplePatternPalette, sizeof(SamplePatternPalette));
            m_ringMem.Unmap();
        }
    }
}

} // Gfx6
} // Pal
