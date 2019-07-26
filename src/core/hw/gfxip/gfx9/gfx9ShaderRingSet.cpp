/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/platform.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRing.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ShaderRingSet::ShaderRingSet(
    Device* pDevice,
    size_t  numRings,      // Number of shader rings contained in this ring-set
    size_t  numSrds)       // Number of SRD's in the ring-set's table
    :
    m_pDevice(pDevice),
    m_numRings(numRings),
    m_numSrds(numSrds),
    m_ppRings(nullptr),
    m_pSrdTable(nullptr),
    m_gfxLevel(m_pDevice->Parent()->ChipProperties().gfxLevel)
{
}

// =====================================================================================================================
ShaderRingSet::~ShaderRingSet()
{
    // Note: The table of ring pointers and SRDs are packed into the same memory allocation.
    if (m_ppRings != nullptr)
    {
        PAL_ASSERT(m_pSrdTable != nullptr);
        m_pSrdTable = nullptr;

        for (size_t idx = 0; idx < m_numRings; ++idx)
        {
            PAL_SAFE_DELETE(m_ppRings[idx], m_pDevice->GetPlatform());
        }

        PAL_SAFE_FREE(m_ppRings, m_pDevice->GetPlatform());
    }

    if (m_srdTableMem.IsBound())
    {
        m_pDevice->Parent()->MemMgr()->FreeGpuMem(m_srdTableMem.Memory(), m_srdTableMem.Offset());
    }
}

// =====================================================================================================================
// Initializes this shader-ring set object.
Result ShaderRingSet::Init()
{
    GpuMemoryCreateInfo srdMemCreateInfo = { };
    srdMemCreateInfo.size      = TotalMemSize();
    srdMemCreateInfo.priority  = GpuMemPriority::Normal;
    srdMemCreateInfo.vaRange   = VaRange::DescriptorTable;

    if (m_pDevice->Parent()->LocalInvDropCpuWrites() == false)
    {
        srdMemCreateInfo.heaps[0]  = GpuHeapLocal;
        srdMemCreateInfo.heaps[1]  = GpuHeapGartUswc;
        srdMemCreateInfo.heaps[2]  = GpuHeapGartCacheable;
        srdMemCreateInfo.heapCount = 3;
    }
    else
    {
        srdMemCreateInfo.heaps[0]  = GpuHeapGartUswc;
        srdMemCreateInfo.heaps[1]  = GpuHeapGartCacheable;
        srdMemCreateInfo.heapCount = 2;
    }

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident = 1;

    GpuMemory* pGpuMemory = nullptr;
    gpusize    memOffset  = 0;

    // Allocate the memory object for each ring-set's SRD table.
    Result result = m_pDevice->Parent()->MemMgr()->AllocateGpuMem(srdMemCreateInfo,
                                                                  internalInfo,
                                                                  0,
                                                                  &pGpuMemory,
                                                                  &memOffset);

    if (result == Result::Success)
    {
        // Assume failure.
        result = Result::ErrorOutOfMemory;

        // Update the video memory binding for our internal SRD table.
        m_srdTableMem.Update(pGpuMemory, memOffset);

        // Allocate memory for the ring pointer table and SRD table.
        const size_t ringTableSize    = (sizeof(ShaderRing*) * m_numRings);
        void*const   pRingSrdTableMem = PAL_CALLOC(ringTableSize + SrdTableSize(),
                                                   m_pDevice->GetPlatform(),
                                                   AllocObject);

        if (pRingSrdTableMem != nullptr)
        {
            m_ppRings   = static_cast<ShaderRing**>(pRingSrdTableMem);
            m_pSrdTable = static_cast<BufferSrd*>(VoidPtrInc(m_ppRings, ringTableSize));
            result      = Result::Success;
        }
    }

    if (result == Result::Success)
    {
        for (size_t idx = 0; idx < m_numRings; ++idx)
        {
            // Allocate the shader ring objects.
            switch (static_cast<ShaderRingType>(idx))
            {
            case ShaderRingType::ComputeScratch:
                m_ppRings[idx] =
                    PAL_NEW(ScratchRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, ShaderCompute);
                break;

            case ShaderRingType::GfxScratch:
                m_ppRings[idx] =
                    PAL_NEW(ScratchRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, ShaderGraphics);
                break;

            case ShaderRingType::GsVs:
                m_ppRings[idx] = PAL_NEW(GsVsRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::TfBuffer:
                m_ppRings[idx] =
                    PAL_NEW(TessFactorBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::OffChipLds:
                m_ppRings[idx] =
                    PAL_NEW(OffchipLdsBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::SamplePos:
                m_ppRings[idx] =
                    PAL_NEW(SamplePosBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            if (m_ppRings[idx] == nullptr)
            {
                result = Result::ErrorOutOfMemory;
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Validates that each ring is large enough to support the specified item-size. This function assumes the associated
// Queue is not busy using this RingSet (i.e., the Queue is idle), so that it is safe to map the SRD table memory.
Result ShaderRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette)
{
    Result result = Result::Success;

    bool updateSrdTable = false;

    for (size_t ring = 0; (result == Result::Success) && (ring < NumRings()); ++ring)
    {
        // It is acceptable for some rings to not exist. However, either the Ring must exist or itemSize must be zero.
        PAL_ASSERT((m_ppRings[ring] != nullptr) || (ringSizes.itemSize[ring] == 0));

        if (m_ppRings[ring] != nullptr)
        {
            if (ringSizes.itemSize[ring] > m_ppRings[ring]->ItemSizeMax())
            {
                // We're increasing the size of this ring, and it will get a new address - force an updated of the SRD
                // table.
                updateSrdTable = true;
            }

            result = m_ppRings[ring]->Validate(ringSizes.itemSize[ring], static_cast<ShaderRingType>(ring));
        }
    }

    if ((result == Result::Success) && updateSrdTable)
    {
        // Need to upload our CPU copy of the SRD table into the SRD table video memory because we validated the TF
        // Buffer up-front, so its SRD needs to be uploaded now.
        void* pData = nullptr;
        result = m_srdTableMem.Map(&pData);

        if (result == Result::Success)
        {
            const size_t srdTableSize = SrdTableSize();

            memcpy(pData, m_pSrdTable, srdTableSize);

            m_srdTableMem.Unmap();
        }
    }

    // Upload sample pattern palette
    SamplePosBuffer* pSamplePosBuf =
        static_cast<SamplePosBuffer*>(m_ppRings[static_cast<size_t>(ShaderRingType::SamplePos)]);
    if (pSamplePosBuf != nullptr)
    {
        pSamplePosBuf->UploadSamplePatternPalette(samplePatternPalette);
    }

    return result;
}

// =====================================================================================================================
UniversalRingSet::UniversalRingSet(
    Device* pDevice)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumUniversal),
                  static_cast<size_t>(ShaderRingSrd::NumUniversal))
{
    BuildPm4Headers();
}

// =====================================================================================================================
// Assembles the PM4 packet headers contained in the image of PM4 commands for this Ring Set.
void UniversalRingSet::BuildPm4Headers()
{
    memset(&m_pm4Commands, 0, sizeof(m_pm4Commands));
    const CmdUtil& cmdUtil        = m_pDevice->CmdUtil();
    const auto&    regInfo        = cmdUtil.GetRegInfo();
    const uint16   baseUserDataHs = m_pDevice->GetBaseUserDataReg(HwShaderStage::Hs);

    // Setup m_pm4Commands
    // Setup packets which issue VS_PARTIAL_FLUSH and VGT_FLUSH events to make sure it is safe to write the ring config
    // registers.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, EngineTypeUniversal, &m_pm4Commands.vsPartialFlush);
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildNonSampleEventWrite(VGT_FLUSH, EngineTypeUniversal, &m_pm4Commands.vgtFlush);

    // Setup the 1st PM4 packet, which sets the config registers VGT_TF_MEMORY_BASE and VGT_TF_MEMORY_BASE_HI.
    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        m_pm4Commands.spaceNeeded += cmdUtil.BuildSetSeqConfigRegs(regInfo.mmVgtTfMemBase,
                                                                   regInfo.mmVgtTfMemBaseHi,
                                                                   &m_pm4Commands.tfMemBase.gfx9.hdrVgtTfMemoryBase);
    }
    else if (IsGfx10(m_gfxLevel))
    {
        m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneConfigReg(regInfo.mmVgtTfMemBase,
                                                                  &m_pm4Commands.tfMemBase.gfx10.hdrVgtTfMemoryBaseLo);

        m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneConfigReg(regInfo.mmVgtTfMemBaseHi,
                                                                  &m_pm4Commands.tfMemBase.gfx10.hdrVgtTfMemoryBaseHi);
    }

    // Setup the 2nd PM4 packet, which sets the config register VGT_TF_RING_SIZE.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildSetOneConfigReg(Gfx09::mmVGT_TF_RING_SIZE, &m_pm4Commands.hdrVgtTfRingSize);

    // Setup the 3rd PM4 packet, which sets the config register VGT_HS_OFFCHIP_PARAM.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildSetOneConfigReg(Gfx09::mmVGT_HS_OFFCHIP_PARAM, &m_pm4Commands.hdrVgtHsOffchipParam);

    // Setup the 4th PM4 packet, which sets the config register VGT_GSVS_RING_SIZE.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildSetOneConfigReg(Gfx09::mmVGT_GSVS_RING_SIZE, &m_pm4Commands.hdrVgtGsVsRingSize);

    // Setup the 5th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_LS_0.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(baseUserDataHs + InternalTblStartReg,
                                                          ShaderGraphics,
                                                          &m_pm4Commands.hdrLsUserData);

    // Setup the 6th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_ES_0;
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(regInfo.mmUserDataStartGsShaderStage + InternalTblStartReg,
                                                          ShaderGraphics,
                                                          &m_pm4Commands.hdrEsUserData);

    // Setup the 7th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_VS_0.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + InternalTblStartReg,
                                                          ShaderGraphics,
                                                          &m_pm4Commands.hdrVsUserData);

    // Setup the 8th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_PS_0.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + InternalTblStartReg,
                                                          ShaderGraphics,
                                                          &m_pm4Commands.hdrPsUserData);

    // Setup the 9th PM4 packet, which sets the compute registers COMPUTE_USER_DATA_0.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                                                          ShaderCompute,
                                                          &m_pm4Commands.hdrComputeUserData);

    // Setup the 10th PM4 packet, which sets the context register SPI_TMPRING_SIZE for Gfx scratch memory.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildSetOneContextReg(mmSPI_TMPRING_SIZE, &m_pm4Commands.hdrGfxScratchRingSize);

    // Setup the 11th PM4 packet, which sets the compute SH register COMPUTE_TMPRING_SIZE for Compute scratch memory.
    m_pm4Commands.spaceNeeded +=
        cmdUtil.BuildSetOneShReg(mmCOMPUTE_TMPRING_SIZE, ShaderCompute, &m_pm4Commands.hdrComputeScratchRingSize);
}

// =====================================================================================================================
// Initializes this Universal-Queue shader-ring set object.
Result UniversalRingSet::Init()
{
    const Pal::Device&  device = *(m_pDevice->Parent());

    // First, call the base class' implementation to allocate and init each Ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Update our PM4 image with the GPU virtual address for the SRD table.
        const uint32 srdTblVirtAddrLo = LowPart(m_srdTableMem.GpuVirtAddr());

        m_pm4Commands.lsUserDataLo                = srdTblVirtAddrLo;
        m_pm4Commands.esUserDataLo                = srdTblVirtAddrLo;
        m_pm4Commands.vsUserDataLo.bits.DATA      = srdTblVirtAddrLo;
        m_pm4Commands.psUserDataLo.bits.DATA      = srdTblVirtAddrLo;
        m_pm4Commands.computeUserDataLo.bits.DATA = srdTblVirtAddrLo;

        // Set up the SPI_TMPRING_SIZE for the graphics shader scratch ring.
        const ScratchRing*const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);

        m_pm4Commands.gfxScratchRingSize.bits.WAVES    = pScratchRingGfx->CalculateWaves();
        m_pm4Commands.gfxScratchRingSize.bits.WAVESIZE = pScratchRingGfx->CalculateWaveSize();

        // Set up the COMPUTE_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_pm4Commands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
        m_pm4Commands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        // The OFFCHIP_GRANULARITY field of VGT_HS_OFFCHIP_PRARM is determined at init-time by the value of the related
        // setting.
        if (IsGfx9(device)
            || IsGfx101(device)
           )
        {
            m_pm4Commands.vgtHsOffchipParam.most.OFFCHIP_GRANULARITY = m_pDevice->Settings().offchipLdsBufferSize;
        }
        else
        {
            // Which GPU is this?
            PAL_ASSERT_ALWAYS();
        }
    }

    if (result == Result::Success)
    {
        // Need to upload our CPU copy of the SRD table into the SRD table video memory because we validated the offchip
        // HW buffers upfront, so its SRD needs to be uploaded now.
        void* pData = nullptr;
        result = m_srdTableMem.Map(&pData);

        if (result == Result::Success)
        {
            const size_t srdTableSize = SrdTableSize();

            memcpy(pData, m_pSrdTable, srdTableSize);

            m_srdTableMem.Unmap();
        }
    }

    return result;
}

// =====================================================================================================================
// Validates that each ring is large enough to support the specified item-size. This function assumes the associated
// Queue is not busy using this RingSet (i.e., the Queue is idle), so that it is safe to map the SRD table memory.
Result UniversalRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette)
{
    const Pal::Device&  device = *(m_pDevice->Parent());

    // First, perform the base class' validation.
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette);

    if (result == Result::Success)
    {
        // Next, update our PM4 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);
        const ScratchRing*const pScratchRingCs  =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);
        const ShaderRing*const  pGsVsRing       = m_ppRings[static_cast<size_t>(ShaderRingType::GsVs)];
        const ShaderRing*const  pTfBuffer       = m_ppRings[static_cast<size_t>(ShaderRingType::TfBuffer)];
        const ShaderRing*const  pOffchipLds     = m_ppRings[static_cast<size_t>(ShaderRingType::OffChipLds)];

        // Scratch rings:
        m_pm4Commands.gfxScratchRingSize.bits.WAVES        = pScratchRingGfx->CalculateWaves();
        m_pm4Commands.gfxScratchRingSize.bits.WAVESIZE     = pScratchRingGfx->CalculateWaveSize();

        m_pm4Commands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
        m_pm4Commands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        // ES/GS and GS/VS ring size registers are in units of 64 DWORD's.
        constexpr uint32 GsRingSizeAlignmentShift = 6;

        m_pm4Commands.vgtGsVsRingSize.bits.MEM_SIZE = (pGsVsRing->MemorySizeDwords() >> GsRingSizeAlignmentShift);

        // Tess-Factor Buffer:
        m_pm4Commands.vgtTfRingSize.bits.SIZE = pTfBuffer->MemorySizeDwords();
        if (pTfBuffer->IsMemoryValid())
        {
            const uint32  addrLo = Get256BAddrLo(pTfBuffer->GpuVirtAddr());
            const uint32  addrHi = Get256BAddrHi(pTfBuffer->GpuVirtAddr());

            if (m_gfxLevel == GfxIpLevel::GfxIp9)
            {
                auto*  pGfx9 = &m_pm4Commands.tfMemBase.gfx9;

                pGfx9->vgtTfMemoryBaseLo.bits.BASE    = addrLo;
                pGfx9->vgtTfMemoryBaseHi.bits.BASE_HI = addrHi;
            }
            else if (IsGfx10(m_gfxLevel))
            {
                auto*  pGfx10 = &m_pm4Commands.tfMemBase.gfx10;

                pGfx10->vgtTfMemoryBaseLo     = addrLo;
                pGfx10->vgtTfMemoryBaseHi     = addrHi;
            }
        }

        // Off-chip LDS Buffers:
        if (pOffchipLds->IsMemoryValid())
        {
            if (IsGfx9(device)
                || IsGfx10(device)
               )
            {
                // OFFCHIP_BUFFERING setting is biased by one (i.e., 0=1, 511=512, etc.).
                m_pm4Commands.vgtHsOffchipParam.most.OFFCHIP_BUFFERING = pOffchipLds->ItemSizeMax() - 1;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Writes our PM4 commands into the specified command stream. Returns the next unused DWORD in pCmdSpace.
uint32* UniversalRingSet::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WritePm4Image(m_pm4Commands.spaceNeeded, &m_pm4Commands, pCmdSpace);
}

// =====================================================================================================================
ComputeRingSet::ComputeRingSet(
    Device* pDevice)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumCompute),
                  static_cast<size_t>(ShaderRingSrd::NumCompute))
{
    BuildPm4Headers();
}

// =====================================================================================================================
// Assembles the PM4 packet headers contained in the image of PM4 commands for this Ring Set.
void ComputeRingSet::BuildPm4Headers()
{
    memset(&m_pm4Commands, 0, sizeof(m_pm4Commands));

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // Setup the 1st PM4 packet, which sets the compute register COMPUTE_USER_DATA_(N).
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                             ShaderCompute,
                             &m_pm4Commands.hdrComputeUserData);

    // Setup the 2nd PM4 packet, which sets the Compute SH register COMPUTE_TMPRING_SIZE for Compute scratch memory.
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_TMPRING_SIZE, ShaderCompute, &m_pm4Commands.hdrComputeScratchRingSize);
}

// =====================================================================================================================
// Initializes this Compute-Queue shader-ring set object.
Result ComputeRingSet::Init()
{
    // First, call the base class' implementation to allocate and init each Ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Update our PM4 image with the GPU virtual address for the SRD table.
        m_pm4Commands.computeUserDataLo.bits.DATA = LowPart(m_srdTableMem.GpuVirtAddr());

        // Set up the SPI_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_pm4Commands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
        m_pm4Commands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();
    }

    return result;
}

// =====================================================================================================================
// Validates that each ring is large enough to support the specified item-size. This function assumes the associated
// Queue is not busy using this RingSet (i.e., the Queue is idle), so that it is safe to map the SRD table memory.
Result ComputeRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette)
{
    // First, perform the base class' validation.
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette);

    if (result == Result::Success)
    {
        // Next, update our PM4 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_pm4Commands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
        m_pm4Commands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();
    }

    return result;
}

// =====================================================================================================================
// Writes our PM4 commands into the specified command stream. Returns the next unused DWORD in pCmdSpace.
uint32* ComputeRingSet::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WritePm4Image(sizeof(m_pm4Commands) / sizeof(uint32), &m_pm4Commands, pCmdSpace);
}

} // Gfx9
} // Pal
