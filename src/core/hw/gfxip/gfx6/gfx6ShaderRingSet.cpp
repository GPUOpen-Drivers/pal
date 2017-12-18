/*
 *******************************************************************************
 *
 * Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
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

#include "core/platform.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6Device.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRing.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRingSet.h"

using namespace Util;

namespace Pal
{
namespace Gfx6
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
    m_pSrdTable(nullptr)
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

            case ShaderRingType::EsGs:
                m_ppRings[idx] =
                    PAL_NEW(EsGsRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::GsVs:
                m_ppRings[idx] =
                    PAL_NEW(GsVsRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::TfBuffer:
                m_ppRings[idx] =
                    PAL_NEW(TessFactorBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            case ShaderRingType::OffChipLds:
                // Only allocate the off-chip LDS buffer if the setting is enabled.
                if (GetGfx6Settings(*m_pDevice->Parent()).numOffchipLdsBuffers > 0)
                {
                    m_ppRings[idx] =
                        PAL_NEW(OffchipLdsBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                }
                break;

            case ShaderRingType::SamplePos:
                m_ppRings[idx] =
                    PAL_NEW(SamplePosBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable);
                break;

            default:
                PAL_ASSERT_ALWAYS();
                break;
            }

            // Note: Not creating a ring object is not a fatal error condition.
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

            result = m_ppRings[ring]->Validate(ringSizes.itemSize[ring],
                                               static_cast<ShaderRingType>(ring));
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
    memset(&m_pm4NonRlcRestoredCommands, 0, sizeof(m_pm4NonRlcRestoredCommands));

    uint32 mmVgtEsGsRingSize;
    uint32 mmVgtGsVsRingSize;
    uint32 mmVgtTfMemoryBase;
    uint32 mmVgtTfRingSize;
    uint32 mmVgtHsOffchipParam;

    if (m_pDevice->Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp6)
    {
        mmVgtEsGsRingSize   = mmVGT_ESGS_RING_SIZE__SI;
        mmVgtGsVsRingSize   = mmVGT_GSVS_RING_SIZE__SI;
        mmVgtTfMemoryBase   = mmVGT_TF_MEMORY_BASE__SI;
        mmVgtTfRingSize     = mmVGT_TF_RING_SIZE__SI;
        mmVgtHsOffchipParam = mmVGT_HS_OFFCHIP_PARAM__SI;
    }
    else
    {
        mmVgtEsGsRingSize   = mmVGT_ESGS_RING_SIZE__CI__VI;
        mmVgtGsVsRingSize   = mmVGT_GSVS_RING_SIZE__CI__VI;
        mmVgtTfMemoryBase   = mmVGT_TF_MEMORY_BASE__CI__VI;
        mmVgtTfRingSize     = mmVGT_TF_RING_SIZE__CI__VI;
        mmVgtHsOffchipParam = mmVGT_HS_OFFCHIP_PARAM__CI__VI;
    }

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    // Setup m_pm4Commands
    // Setup packets which issue VS_PARTIAL_FLUSH and VGT_FLUSH events to make sure it is safe to write the ring config
    // registers.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildEventWrite(VS_PARTIAL_FLUSH, &m_pm4Commands.vsPartialFlush);
    m_pm4Commands.spaceNeeded += cmdUtil.BuildEventWrite(VGT_FLUSH, &m_pm4Commands.vgtFlush);

    // Setup the 1st PM4 packet, which sets the config registers VGT_ESGS_RING_SIZE and VGT_GSVS_RING_SIZE.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetSeqConfigRegs(mmVgtEsGsRingSize,
                                                               mmVgtGsVsRingSize,
                                                               &m_pm4Commands.hdrEsGsGsVsRingSize);

    // Setup the 2nd PM4 packet, which sets the config register VGT_TF_MEMORY_BASE.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneConfigReg(mmVgtTfMemoryBase, &m_pm4Commands.hdrTfMemoryBase);

    // Setup the 3rd PM4 packet, which sets the config register VGT_TF_RING_SIZE.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneConfigReg(mmVgtTfRingSize, &m_pm4Commands.hdrTfRingSize);

    // Setup the 4th PM4 packet, which sets the config register VGT_HS_OFFCHIP_PARAM.
    m_pm4Commands.spaceNeeded += cmdUtil.BuildSetOneConfigReg(mmVgtHsOffchipParam, &m_pm4Commands.hdrHsOffchipParam);

    // Setup m_pm4NonRlcRestoredCommands
    // Setup the 1th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_LS_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_LS_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrLsUserData);

    // Setup the 2nd PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_HS_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_HS_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrHsUserData);

    // Setup the 3rd PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_ES_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_ES_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrEsUserData);

    // Setup the 4th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_GS_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_GS_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrGsUserData);

    // Setup the 5th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_VS_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_VS_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrVsUserData);

    // Setup the 6th PM4 packet, which sets the graphics SH registers SPI_SHADER_USER_DATA_PS_0.
    cmdUtil.BuildSetOneShReg(mmSPI_SHADER_USER_DATA_PS_0 + InternalTblStartReg,
                             ShaderGraphics,
                             &m_pm4NonRlcRestoredCommands.hdrPsUserData);

    // Setup the 7th PM4 packet, which sets the compute registers COMPUTE_USER_DATA_0.
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                             ShaderCompute,
                             &m_pm4NonRlcRestoredCommands.hdrComputeUserData);

    // Setup the 8th PM4 packet, which sets the context register SPI_TMPRING_SIZE for Gfx scratch memory.
    cmdUtil.BuildSetOneContextReg(mmSPI_TMPRING_SIZE, &m_pm4NonRlcRestoredCommands.hdrGfxScratchRingSize);

    // Setup the 9th PM4 packet, which sets the compute SH register COMPUTE_TMPRING_SIZE for Compute scratch memory.
    cmdUtil.BuildSetOneShReg(mmCOMPUTE_TMPRING_SIZE, ShaderCompute,
                             &m_pm4NonRlcRestoredCommands.hdrComputeScratchRingSize);
}

// =====================================================================================================================
// Initializes this Universal-Queue shader-ring set object.
Result UniversalRingSet::Init()
{
    // First, call the base class' implementation to allocate and init each Ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Update our PM4 image with the GPU virtual address for the SRD table.
        const uint32 srdTblVirtAddrLo = LowPart(m_srdTableMem.GpuVirtAddr());

        m_pm4NonRlcRestoredCommands.lsUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.hsUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.esUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.gsUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.vsUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.psUserData0.bits.DATA      = srdTblVirtAddrLo;
        m_pm4NonRlcRestoredCommands.computeUserData0.bits.DATA = srdTblVirtAddrLo;

        // Set up the SPI_TMPRING_SIZE for the graphics shader scratch ring.
        const ScratchRing*const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);

        m_pm4NonRlcRestoredCommands.gfxScratchRingSize.bits.WAVES    = pScratchRingGfx->GetNumWaves();
        m_pm4NonRlcRestoredCommands.gfxScratchRingSize.bits.WAVESIZE = pScratchRingGfx->CalculateWaveSize();

        // Set up the COMPUTE_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_pm4NonRlcRestoredCommands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->GetNumWaves();
        m_pm4NonRlcRestoredCommands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        // The OFFCHIP_GRANULARITY field of VGT_HS_OFFCHIP_PRARM is determined at init-time by the value of the related
        // setting.
        m_pm4Commands.vgtHsOffchipParam.bits.OFFCHIP_GRANULARITY__CI__VI =
            m_pDevice->Settings().gfx7OffchipLdsBufferSize;
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
    // First, perform the base class' validation.
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette);

    // PM4 image update if this is not sample position buffer udpate
    if (result == Result::Success)
    {
        // Next, update our PM4 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);
        const ScratchRing*const pScratchRingCs  =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);
        const ShaderRing*const  pEsGsRing       = m_ppRings[static_cast<size_t>(ShaderRingType::EsGs)];
        const ShaderRing*const  pGsVsRing       = m_ppRings[static_cast<size_t>(ShaderRingType::GsVs)];
        const ShaderRing*const  pTfBuffer       = m_ppRings[static_cast<size_t>(ShaderRingType::TfBuffer)];
        const ShaderRing*const  pOffchipLds     = m_ppRings[static_cast<size_t>(ShaderRingType::OffChipLds)];

        // Scratch rings:
        m_pm4NonRlcRestoredCommands.gfxScratchRingSize.bits.WAVESIZE     = pScratchRingGfx->CalculateWaveSize();
        m_pm4NonRlcRestoredCommands.computeScratchRingSize.bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        const auto& chipProps = m_pDevice->Parent()->ChipProperties();

        // ES/GS and GS/VS ring size registers are in units of 64 DWORD's.
        // Note that the ring size per shader engine must be less than 64MB.
        constexpr uint32 GsRingSizeAlignmentShift = 6;
        constexpr uint32 SixtyFourMbInDwords      = 0x1000000;
        constexpr uint32 GsMaxRingSizePerSe       = (SixtyFourMbInDwords >> GsRingSizeAlignmentShift) - 1;

        const gpusize gsMaxRingSize = GsMaxRingSizePerSe * chipProps.gfx6.numShaderEngines;
        const gpusize esGsRingSize  = pEsGsRing->MemorySizeDwords() >> GsRingSizeAlignmentShift;
        const gpusize gsVsRingSize  = pGsVsRing->MemorySizeDwords() >> GsRingSizeAlignmentShift;

        m_pm4Commands.vgtEsGsRingSize.bits.MEM_SIZE = Min(gsMaxRingSize, esGsRingSize);
        m_pm4Commands.vgtGsVsRingSize.bits.MEM_SIZE = Min(gsMaxRingSize, gsVsRingSize);

        // Tess-Factor Buffer:
        m_pm4Commands.vgtTfRingSize.bits.SIZE = pTfBuffer->MemorySizeDwords();
        if (pTfBuffer->IsMemoryValid())
        {
            m_pm4Commands.vgtTfMemoryBase.bits.BASE = Get256BAddrLo(pTfBuffer->GpuVirtAddr());
        }

        // Off-chip LDS Buffers:
        // NOTE: For Iceland and Hainan, it's generally faster to use on-chip tess for these ASICs due to
        //       their low memory bandwidth. So the off-chip LDS buffer will be disabled and pOffchipLds won't be
        //       allocated space. Need to check this first.
        if ((pOffchipLds != nullptr) && pOffchipLds->IsMemoryValid())
        {
            m_pm4Commands.vgtHsOffchipParam.bits.OFFCHIP_BUFFERING = pOffchipLds->ItemSizeMax();
            if ((chipProps.gfxLevel != GfxIpLevel::GfxIp6) && (chipProps.gfxLevel != GfxIpLevel::GfxIp7))
            {
                // On GFXIP8 and newer, the OFFCHIP_BUFFERING setting is biased by one (i.e., 0=1, 511=512, etc.).
                m_pm4Commands.vgtHsOffchipParam.bits.OFFCHIP_BUFFERING--;
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
// Write affected registers not in the Rlc save/restore list. Returns the next unused DWORD in pCmdSpace.
uint32* UniversalRingSet::WriteNonRlcRestoredRegs(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WritePm4Image(sizeof(m_pm4NonRlcRestoredCommands) / sizeof(uint32),
                                     &m_pm4NonRlcRestoredCommands,
                                     pCmdSpace);
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

    // Setup the 1st PM4 packet, which sets the compute registers COMPUTE_USER_DATA_0.
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
        m_pm4Commands.computeUserData0.bits.DATA = LowPart(m_srdTableMem.GpuVirtAddr());

        // Set up the SPI_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_pm4Commands.computeScratchRingSize.bits.WAVES    = pScratchRingCs->GetNumWaves();
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

} // Gfx6
} // Pal
