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
#include "core/queue.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12Device.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRing.h"
#include "core/hw/gfxip/gfx12/gfx12ShaderRingSet.h"
#include "palDequeImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
ShaderRingSet::ShaderRingSet(
    Device* pDevice,
    size_t  numRings,    // Number of shader rings contained in this ring-set
    size_t  numSrds,     // Number of SRD's in the ring-set's table
    bool    tmzEnabled)  // Is this shader ring TMZ protected
    :
    m_pDevice(pDevice),
    m_numRings(numRings),
    m_numSrds(numSrds),
    m_tmzEnabled(tmzEnabled),
    m_ppRings(nullptr),
    m_pSrdTable(nullptr),
    m_deferredFreeMemDeque(pDevice->GetPlatform())
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
    Platform* pPlatform = m_pDevice->GetPlatform();

    Result result = AllocateSrdTableMem();

    if (result == Result::Success)
    {
        // Allocate memory for the ring pointer table and SRD table.
        const size_t ringTableSize    = (sizeof(ShaderRing*) * m_numRings);
        void*const   pRingSrdTableMem = PAL_CALLOC(ringTableSize + SrdTableSize(), pPlatform, AllocObject);

        if (pRingSrdTableMem != nullptr)
        {
            m_ppRings   = static_cast<ShaderRing**>(pRingSrdTableMem);
            m_pSrdTable = static_cast<sq_buf_rsrc_t*>(VoidPtrInc(m_ppRings, ringTableSize));
            result      = Result::Success;
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if (result == Result::Success)
    {
        for (size_t idx = 0; idx < m_numRings; ++idx)
        {
            // Allocate the shader ring objects.
            switch (static_cast<ShaderRingType>(idx))
            {
            case ShaderRingType::VertexAttributes:
                m_ppRings[idx] = PAL_NEW(VertexAttributeRing, pPlatform, AllocObject)(m_pDevice,
                                                                                      m_pSrdTable,
                                                                                      m_tmzEnabled);
                break;
            case ShaderRingType::SamplePos:
                m_ppRings[idx] =
                    PAL_NEW(SamplePosBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice,
                                                                                    m_pSrdTable,
                                                                                    m_tmzEnabled);
                break;
            case ShaderRingType::TfBuffer:
                m_ppRings[idx] = PAL_NEW(TfBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice,
                                                                                          m_pSrdTable,
                                                                                          m_tmzEnabled);
                break;
            case ShaderRingType::OffChipLds:
                m_ppRings[idx] = PAL_NEW(OffChipLds, m_pDevice->GetPlatform(), AllocObject)(m_pDevice,
                                                                                            m_pSrdTable,
                                                                                            m_tmzEnabled);
                break;
            case ShaderRingType::ComputeScratch:
                m_ppRings[idx] = PAL_NEW(ScratchRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice,
                                                                                             m_pSrdTable,
                                                                                             true,
                                                                                             m_tmzEnabled);
                break;
            case ShaderRingType::GfxScratch:
                m_ppRings[idx] = PAL_NEW(ScratchRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice,
                                                                                             m_pSrdTable,
                                                                                             false,
                                                                                             m_tmzEnabled);
                break;
            case ShaderRingType::PrimBuffer:
                m_ppRings[idx] = PAL_NEW(PrimBufferRing, pPlatform, AllocObject)(m_pDevice,
                                                                                 m_pSrdTable,
                                                                                 m_tmzEnabled);
                break;
            case ShaderRingType::PosBuffer:
                m_ppRings[idx] = PAL_NEW(PosBufferRing, pPlatform, AllocObject)(m_pDevice,
                                                                                m_pSrdTable,
                                                                                m_tmzEnabled);
                break;
            case ShaderRingType::PayloadData:
                m_ppRings[idx] = PAL_NEW(PayloadDataRing, pPlatform, AllocObject)(m_pDevice,
                                                                                  m_pSrdTable,
                                                                                  m_tmzEnabled);
                break;
            case ShaderRingType::MeshScratch:
                m_ppRings[idx] = PAL_NEW(MeshScratchRing, pPlatform, AllocObject)(m_pDevice,
                                                                                  m_pSrdTable,
                                                                                  m_tmzEnabled);
                break;
            case ShaderRingType::TaskMeshCtrlDrawRing:
                m_ppRings[idx] = PAL_NEW(TaskMeshCtrlDrawRing, pPlatform, AllocObject)(m_pDevice,
                                                                                       m_pSrdTable);
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
// Validates that each ring is large enough to support the specified item-size.
Result ShaderRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette,
    uint64                      lastTimeStamp,
    uint32*                     pReallocatedRings)
{
    Result result = Result::Success;

    bool updateSrdTable    = false;
    bool deferFreeSrdTable = false;

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

            ShaderRingMemory deferredMem = {nullptr, 0, lastTimeStamp};
            result = m_ppRings[ring]->Validate(ringSizes.itemSize[ring], &deferredMem);

            if (deferredMem.pGpuMemory != nullptr)
            {
                // If any shader ring needs to defer free ring memory, the current shader SRD table needs to be
                // defer freed as well.
                deferFreeSrdTable = true;
                m_deferredFreeMemDeque.PushBack(deferredMem);
                updateSrdTable = true;
            }

            if (updateSrdTable && deferFreeSrdTable)
            {
                (*pReallocatedRings) |= (1 << ring);
            }
        }
    }

    if ((result == Result::Success) && updateSrdTable)
    {
        result = UpdateSrdTable(deferFreeSrdTable, lastTimeStamp);
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
Result ShaderRingSet::UpdateSrdTable(
    bool   deferFreeSrdTable,
    uint64 lastTimestamp)
{
    Result result = Result::Success;

    if (deferFreeSrdTable)
    {
        // Save the current shardTable, since it might still be referenced by in-flight command buffers.
        ShaderRingMemory ringMem = { m_srdTableMem.Memory(), m_srdTableMem.Offset(), lastTimestamp };
        m_deferredFreeMemDeque.PushBack(ringMem);
        m_srdTableMem.Update(nullptr, 0);

        result = AllocateSrdTableMem();
    }

    void* pData = nullptr;

    if (result == Result::Success)
    {
        result = m_srdTableMem.Map(&pData);
    }

    if (result == Result::Success)
    {
        memcpy(pData, m_pSrdTable, SrdTableSize());
        m_srdTableMem.Unmap();
    }

    return result;
}

// =====================================================================================================================
void ShaderRingSet::ClearDeferredFreeMemory(
    SubmissionContext* pSubmissionCtx)
{
    InternalMemMgr* const pMemMgr = m_pDevice->Parent()->MemMgr();
    while (m_deferredFreeMemDeque.NumElements() > 0)
    {
        const ShaderRingMemory& ringMem = m_deferredFreeMemDeque.Front();

        if (pSubmissionCtx->IsTimestampRetired(ringMem.timestamp))
        {
            if (ringMem.pGpuMemory != nullptr)
            {
                pMemMgr->FreeGpuMem(ringMem.pGpuMemory, ringMem.offset);
            }
            m_deferredFreeMemDeque.PopFront(nullptr);
        }
        else
        {
            break;
        }
    }
}

// =====================================================================================================================
Result ShaderRingSet::AllocateSrdTableMem()
{
    // Allocate a new shaderTable
    GpuMemoryCreateInfo srdMemCreateInfo = { };
    srdMemCreateInfo.size        = TotalMemSize();
    srdMemCreateInfo.priority    = GpuMemPriority::Normal;
    srdMemCreateInfo.vaRange     = VaRange::DescriptorTable;
    srdMemCreateInfo.heapAccess  = GpuHeapAccess::GpuHeapAccessGpuMostly;

    GpuMemoryInternalCreateInfo internalInfo = { };
    internalInfo.flags.alwaysResident = 1;

    GpuMemory* pGpuMemory = nullptr;
    gpusize    memOffset  = 0;

    // Allocate the memory object for each ring-set's SRD table.
    InternalMemMgr* const pMemMgr = m_pDevice->Parent()->MemMgr();
    Result result = pMemMgr->AllocateGpuMem(srdMemCreateInfo, internalInfo, 0, &pGpuMemory, &memOffset);

    if (result == Result::Success)
    {
        // Update the video memory binding for our internal SRD table.
        m_srdTableMem.Update(pGpuMemory, memOffset);
    }

    return result;
}

// =====================================================================================================================
void ShaderRingSet::CopySrdTableEntry(
    ShaderRingSrd  entry,
    sq_buf_rsrc_t* pSrdTable)
{
    size_t entryIdx = size_t(entry);

    // We need to make sure that the entry can be properly placed in our SRD table.
    // This is for cases where we copy a ring SRD but don't own the ring ourselves.
    PAL_ASSERT(size_t(entryIdx) < m_numSrds);

    memcpy(&m_pSrdTable[entryIdx], &pSrdTable[entryIdx], sizeof(sq_buf_rsrc_t));
}

// =====================================================================================================================
UniversalRingSet::UniversalRingSet(
    Device* pDevice,
    bool    isTmz)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumUniversal),
                  static_cast<size_t>(ShaderRingSrd::NumUniversal),
                  isTmz),
    m_gfxRing{},
    m_csRing{},
    m_pAceRingSet(nullptr)
{
    CsRingSet::Init(m_csRing);
    GfxRingSet::Init(m_gfxRing);
}

// =====================================================================================================================
UniversalRingSet::~UniversalRingSet()
{
    if (m_pAceRingSet != nullptr)
    {
        PAL_SAFE_DELETE(m_pAceRingSet, m_pDevice->GetPlatform());
    }
}

// =====================================================================================================================
// Initializes this Universal-Queue shader-ring set object.
Result UniversalRingSet::Init()
{
    // First, call the base class' implementation to allocate and init each ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Set up the SPI_TMPRING_SIZE for the graphics shader scratch ring.
        const ScratchRing* const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);

        auto* pGfxScratchRingSize = GfxRingSet::Get<mmSPI_TMPRING_SIZE, SPI_TMPRING_SIZE>(m_gfxRing);
        pGfxScratchRingSize->bits.WAVES    = pScratchRingGfx->CalculateWaves();
        pGfxScratchRingSize->bits.WAVESIZE = pScratchRingGfx->CalculateWaveSize();

        // Set up the COMPUTE_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing* const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        auto* pCsScratchRingSize = CsRingSet::Get<mmCOMPUTE_TMPRING_SIZE, COMPUTE_TMPRING_SIZE>(m_csRing);
        pCsScratchRingSize->bits.WAVES    = pScratchRingCs->CalculateWaves();
        pCsScratchRingSize->bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        auto* pVgtHsOffChipParam = GfxRingSet::Get<mmVGT_HS_OFFCHIP_PARAM, VGT_HS_OFFCHIP_PARAM>(m_gfxRing);
        pVgtHsOffChipParam->bits.OFFCHIP_GRANULARITY =
            m_pDevice->Parent()->Settings().offchipLdsBufferSize;

        auto* pAttributeRingSize = GfxRingSet::Get<mmSPI_ATTRIBUTE_RING_SIZE, SPI_ATTRIBUTE_RING_SIZE>(m_gfxRing);
        pAttributeRingSize->bits.L1_POLICY = GL1_CACHE_POLICY_MISS_EVICT;

        auto* pPrimRingSize = GfxRingSet::Get<mmGE_PRIM_RING_SIZE, GE_PRIM_RING_SIZE>(m_gfxRing);
        pPrimRingSize->u32All = mmGE_PRIM_RING_SIZE_DEFAULT;
        pPrimRingSize->bits.PAB_TEMPORAL = m_pDevice->Settings().gfx12TemporalHintsPhqRead;
        pPrimRingSize->bits.PAF_TEMPORAL = m_pDevice->Settings().gfx12TemporalHintsPhqWrite;

        auto* pPosRingSize = GfxRingSet::Get<mmGE_POS_RING_SIZE, GE_POS_RING_SIZE>(m_gfxRing);
        pPosRingSize->u32All = mmGE_POS_RING_SIZE_DEFAULT;

        // Upload an initial (uninteresting) copy of the SRD table into the SRD table video memory.
        void* pData = nullptr;
        result = m_srdTableMem.Map(&pData);

        if (result == Result::Success)
        {
            memcpy(pData, m_pSrdTable, SrdTableSize());
            m_srdTableMem.Unmap();
        }
    }

    return result;
}

// =====================================================================================================================
// Validates that each ring is large enough to support the specified item-size.
Result UniversalRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette,
    uint64                      lastTimeStamp,
    bool                        hasAce)
{
    const PalSettings& settings = m_pDevice->Parent()->Settings();

    // Check if the TaskMesh control draw ring has already been initialized.
    const bool tsMsCtrlDrawInitialized =
        m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)]->IsMemoryValid();

    // First, perform the base class' validation.
    uint32 reallocatedRings = 0;
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette, lastTimeStamp, &reallocatedRings);

    const bool drawDataReAlloc =
        Util::TestAnyFlagSet(reallocatedRings, (1 << static_cast<uint32>(ShaderRingType::TaskMeshCtrlDrawRing))) ||
        Util::TestAnyFlagSet(reallocatedRings, (1 << static_cast<uint32>(ShaderRingType::PayloadData)));

    // Initialize the task shader control buffer and draw ring after they have been allocated.
    // Also, if we re-allocate the draw and/or payload data rings, we must ensure that all task shader-related
    // rings are re-allocated at the same time and re-initialized.
    TaskMeshCtrlDrawRing* pTaskMeshCtrlDrawRing =
        static_cast<TaskMeshCtrlDrawRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)]);

    const bool tsMsControlBufferInit =
        ((tsMsCtrlDrawInitialized == false) || drawDataReAlloc) && pTaskMeshCtrlDrawRing->IsMemoryValid();

    if (tsMsControlBufferInit)
    {
        pTaskMeshCtrlDrawRing->InitializeControlBufferAndDrawRingBuffer();
    }

    if (result == Result::Success)
    {
        const uint32 srdTableLo = LowPart(m_srdTableMem.GpuVirtAddr());
        GfxRingSet::Get<mmSPI_SHADER_USER_DATA_HS_0, SPI_SHADER_USER_DATA_HS_0>(m_gfxRing)->bits.DATA = srdTableLo;
        GfxRingSet::Get<mmSPI_SHADER_USER_DATA_GS_0, SPI_SHADER_USER_DATA_GS_0>(m_gfxRing)->bits.DATA = srdTableLo;
        GfxRingSet::Get<mmSPI_SHADER_USER_DATA_PS_0, SPI_SHADER_USER_DATA_PS_0>(m_gfxRing)->bits.DATA = srdTableLo;

        CsRingSet::Get<mmCOMPUTE_USER_DATA_0, COMPUTE_USER_DATA_0>(m_csRing)->bits.DATA = srdTableLo;

        const uint32 numSes = m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;

        const ScratchRing* const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);
        const ScratchRing* const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        // Scratch rings:
        auto* pGfxScratchRingSize = GfxRingSet::Get<mmSPI_TMPRING_SIZE, SPI_TMPRING_SIZE>(m_gfxRing);
        pGfxScratchRingSize->bits.WAVES    = pScratchRingGfx->CalculateWaves();
        pGfxScratchRingSize->bits.WAVESIZE = pScratchRingGfx->CalculateWaveSize();

        if (pScratchRingGfx->IsMemoryValid())
        {
            GfxRingSet::Get<mmSPI_GFX_SCRATCH_BASE_LO, SPI_GFX_SCRATCH_BASE_LO>(m_gfxRing)->bits.DATA =
                Get256BAddrLo(pScratchRingGfx->GpuVirtAddr());
            GfxRingSet::Get<mmSPI_GFX_SCRATCH_BASE_HI, SPI_GFX_SCRATCH_BASE_HI>(m_gfxRing)->bits.DATA =
                Get256BAddrHi(pScratchRingGfx->GpuVirtAddr());
        }

        auto* pCsScratchRingSize = CsRingSet::Get<mmCOMPUTE_TMPRING_SIZE, COMPUTE_TMPRING_SIZE>(m_csRing);
        pCsScratchRingSize->bits.WAVES    = pScratchRingCs->CalculateWaves();
        pCsScratchRingSize->bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        if (pScratchRingCs->IsMemoryValid())
        {
            CsRingSet::Get<mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO, COMPUTE_DISPATCH_SCRATCH_BASE_LO>(m_csRing)->bits.DATA =
                Get256BAddrLo(pScratchRingCs->GpuVirtAddr());
            CsRingSet::Get<mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI, COMPUTE_DISPATCH_SCRATCH_BASE_HI>(m_csRing)->bits.DATA =
                Get256BAddrHi(pScratchRingCs->GpuVirtAddr());
        }

        const ShaderRing* const pAttribThruMem = m_ppRings[static_cast<size_t>(ShaderRingType::VertexAttributes)];
        if (pAttribThruMem->IsMemoryValid())
        {
            // AttribThruMem addr and size Gfx12 fields are in units of 64KB.
            constexpr uint32 AttribThruMemShift = 16;

            GfxRingSet::Get<mmSPI_ATTRIBUTE_RING_BASE, SPI_ATTRIBUTE_RING_BASE>(m_gfxRing)->bits.BASE =
                pAttribThruMem->GpuVirtAddr() >> AttribThruMemShift;

            auto* pAttributeRingSize = GfxRingSet::Get<mmSPI_ATTRIBUTE_RING_SIZE, SPI_ATTRIBUTE_RING_SIZE>(m_gfxRing);
            // Size field is biased by 1. This the size per SE
            pAttributeRingSize->bits.MEM_SIZE =
                ((pAttribThruMem->MemorySizeBytes() / numSes) >> AttribThruMemShift) - 1;
        }

        const ShaderRing* const pTfBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::TfBuffer)];
        if (pTfBuffer->IsMemoryValid())
        {
            const uint32  addrLo = Get256BAddrLo(pTfBuffer->GpuVirtAddr());
            const uint32  addrHi = Get256BAddrHi(pTfBuffer->GpuVirtAddr());

            GfxRingSet::Get<mmVGT_TF_MEMORY_BASE, VGT_TF_MEMORY_BASE>(m_gfxRing)->bits.BASE          = addrLo;
            GfxRingSet::Get<mmVGT_TF_MEMORY_BASE_HI, VGT_TF_MEMORY_BASE_HI>(m_gfxRing)->bits.BASE_HI = addrHi;

            auto* pVgtTfRingSize = GfxRingSet::Get<mmVGT_TF_RING_SIZE, VGT_TF_RING_SIZE>(m_gfxRing);
            pVgtTfRingSize->bits.SIZE = pTfBuffer->MemorySizeBytes() / numSes / sizeof(uint32);
        }

        const ShaderRing* const pOffChipLds = m_ppRings[static_cast<size_t>(ShaderRingType::OffChipLds)];
        if (pOffChipLds->IsMemoryValid())
        {
            auto* pVgtHsOffChipParam = GfxRingSet::Get<mmVGT_HS_OFFCHIP_PARAM, VGT_HS_OFFCHIP_PARAM>(m_gfxRing);
            pVgtHsOffChipParam->bits.OFFCHIP_BUFFERING   = (pOffChipLds->ItemSizeMax() / numSes) - 1;
            pVgtHsOffChipParam->bits.OFFCHIP_GRANULARITY = settings.offchipLdsBufferSize;
        }

        const ShaderRing* const pPrimBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::PrimBuffer)];
        if (pPrimBuffer->IsMemoryValid())
        {
            const uint32 addr = LowPart(pPrimBuffer->GpuVirtAddr() >> GeometryExportRingShift);

            GfxRingSet::Get<mmGE_PRIM_RING_BASE, GE_PRIM_RING_BASE>(m_gfxRing)->bits.BASE = addr;
            auto* pPrimRingSize = GfxRingSet::Get<mmGE_PRIM_RING_SIZE, GE_PRIM_RING_SIZE>(m_gfxRing);
            pPrimRingSize->bits.MEM_SIZE = m_pDevice->GeomExportBufferMemSize(m_pDevice->PrimBufferTotalMemSize());
        }

        const ShaderRing* const pPosBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::PosBuffer)];
        if (pPosBuffer->IsMemoryValid())
        {
            const uint32 addr = LowPart(pPosBuffer->GpuVirtAddr() >> GeometryExportRingShift);

            GfxRingSet::Get<mmGE_POS_RING_BASE, GE_POS_RING_BASE>(m_gfxRing)->bits.BASE = addr;
            auto* pPosRingSize = GfxRingSet::Get<mmGE_POS_RING_SIZE, GE_POS_RING_SIZE>(m_gfxRing);
            pPosRingSize->bits.MEM_SIZE = m_pDevice->GeomExportBufferMemSize(m_pDevice->PosBufferTotalMemSize());
        }
    }

    if ((result == Result::Success) && hasAce && (m_pAceRingSet == nullptr))
    {
        m_pAceRingSet =
            PAL_NEW(ComputeRingSet, m_pDevice->GetPlatform(), AllocInternal)(m_pDevice,
                                                                             m_tmzEnabled,
                                                                             size_t(ShaderRingSrd::NumUniversal));

        if (m_pAceRingSet != nullptr)
        {
            result = m_pAceRingSet->Init();
        }
        else
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    if ((result == Result::Success) && hasAce && (m_pAceRingSet != nullptr))
    {
        result = m_pAceRingSet->Validate(ringSizes, samplePatternPalette, lastTimeStamp);

        // If rings were reallocated, we may need to update the SRD table for the PayloadDataRing and DrawDataRing.
        if ((result == Result::Success) && tsMsControlBufferInit)
        {
            // The DrawRing and the PayloadDataRing are both shared with the ACE side.
            m_pAceRingSet->CopySrdTableEntry(ShaderRingSrd::DrawDataRing,    m_pSrdTable);
            m_pAceRingSet->CopySrdTableEntry(ShaderRingSrd::PayloadDataRing, m_pSrdTable);

            result = m_pAceRingSet->UpdateSrdTable((m_deferredFreeMemDeque.NumElements() != 0), lastTimeStamp);
        }
    }

    return result;
}

// =====================================================================================================================
// Writes our PM4 commands into the specified command stream. Returns the next unused DWORD in pCmdSpace.
uint32* UniversalRingSet::WriteCommands(
    uint32* pCmdSpace
    ) const
{
    static_assert(GfxRingSet::FirstContextIdx() != UINT32_MAX, "Must be at least one context register!");
    static_assert(GfxRingSet::FirstShIdx()      != UINT32_MAX, "Must be at least one sh register!");
    static_assert(GfxRingSet::FirstOtherIdx()   != UINT32_MAX, "Must be at least one uconfig register!");

    pCmdSpace = CmdStream::WriteSetContextPairs(&m_gfxRing[GfxRingSet::FirstContextIdx()],
                                                GfxRingSet::NumContext(),
                                                pCmdSpace);
    pCmdSpace = CmdStream::WriteSetShPairs(&m_gfxRing[GfxRingSet::FirstShIdx()],
                                           GfxRingSet::NumSh(),
                                           pCmdSpace);
    pCmdSpace = CmdStream::WriteSetUConfigPairs(&m_gfxRing[GfxRingSet::FirstOtherIdx()],
                                                GfxRingSet::NumOther(),
                                                pCmdSpace);

    static_assert(CsRingSet::NumSh() == CsRingSet::Size(), "CS Ring Set should only have SH regs.");
    pCmdSpace = CmdStream::WriteSetShPairs<ShaderCompute>(&m_csRing[CsRingSet::FirstShIdx()],
                                                          CsRingSet::NumSh(),
                                                          pCmdSpace);

    const ShaderRing* const  pControlBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)];
    if (pControlBuffer->IsMemoryValid())
    {
        pCmdSpace += CmdUtil::BuildTaskStateInit(
            pControlBuffer->GpuVirtAddr(), PredDisable, ShaderGraphics, pCmdSpace);
    }

    return pCmdSpace;
}

// =====================================================================================================================
// Writes the compute portion of this ShaderRingSet into the command stream provided. This is used by the ACE-GFX gang
// submit, where ACE commands are submitted together with GFX in the DE command stream.
uint32* UniversalRingSet::WriteComputeCommands(
    uint32*    pCmdSpace
    ) const
{
    PAL_ASSERT(m_pAceRingSet != nullptr);

    const ShaderRing* const pControlBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)];
    if (pControlBuffer->IsMemoryValid())
    {
        pCmdSpace += CmdUtil::BuildTaskStateInit(
            pControlBuffer->GpuVirtAddr(), PredDisable, ShaderCompute, pCmdSpace);
    }

    pCmdSpace = m_pAceRingSet->WriteCommands(pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
ComputeRingSet::ComputeRingSet(
    Device* pDevice,
    bool    isTmz,
    size_t  numSrds)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumCompute),
                  numSrds,
                  isTmz),
    m_csRing{}
{
    CsRingSet::Init(m_csRing);
}

// =====================================================================================================================
// Initializes this Compute-Queue shader-ring set object.
Result ComputeRingSet::Init()
{
    // First, call the base class' implementation to allocate and init each Ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Set up the SPI_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        auto* pComputeTmpringSize = CsRingSet::Get<mmCOMPUTE_TMPRING_SIZE, COMPUTE_TMPRING_SIZE>(m_csRing);
        pComputeTmpringSize->bits.WAVES    = pScratchRingCs->CalculateWaves();
        pComputeTmpringSize->bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        // Upload an initial (uninteresting) copy of the SRD table into the SRD table video memory.
        void* pData = nullptr;
        result = m_srdTableMem.Map(&pData);

        if (result == Result::Success)
        {
            memcpy(pData, m_pSrdTable, SrdTableSize());
            m_srdTableMem.Unmap();
        }
    }

    return result;
}

// =====================================================================================================================
// Validates that each ring is large enough to support the specified item-size. This function assumes the associated
// Queue is not busy using this RingSet (i.e., the Queue is idle), so that it is safe to map the SRD table memory.
Result ComputeRingSet::Validate(
    const ShaderRingItemSizes&  ringSizes,
    const SamplePatternPalette& samplePatternPalette,
    uint64                      lastTimeStamp)
{
    // First, perform the base class' validation.
    uint32 reallocatedRings = 0;
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette, lastTimeStamp, &reallocatedRings);

    if (result == Result::Success)
    {
        CsRingSet::Get<mmCOMPUTE_USER_DATA_0, COMPUTE_USER_DATA_0>(m_csRing)->bits.DATA =
            LowPart(m_srdTableMem.GpuVirtAddr());

        // Next, update our Gfx12 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        auto* pComputeTmpringSize = CsRingSet::Get<mmCOMPUTE_TMPRING_SIZE, COMPUTE_TMPRING_SIZE>(m_csRing);
        pComputeTmpringSize->bits.WAVES    = pScratchRingCs->CalculateWaves();
        pComputeTmpringSize->bits.WAVESIZE = pScratchRingCs->CalculateWaveSize();

        if (pScratchRingCs->IsMemoryValid())
        {
            CsRingSet::Get<mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO, COMPUTE_DISPATCH_SCRATCH_BASE_LO>(m_csRing)->bits.DATA =
                Get256BAddrLo(pScratchRingCs->GpuVirtAddr());
            CsRingSet::Get<mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI, COMPUTE_DISPATCH_SCRATCH_BASE_HI>(m_csRing)->bits.DATA =
                Get256BAddrHi(pScratchRingCs->GpuVirtAddr());
        }
    }

    return result;
}

// =====================================================================================================================
// Writes our PM4 commands into the specified command stream. Returns the next unused DWORD in pCmdSpace.
uint32* ComputeRingSet::WriteCommands(
    uint32* pCmdSpace
    ) const
{
    static_assert(CsRingSet::NumSh() == CsRingSet::Size(), "CS Ring Set should only have SH regs.");
    pCmdSpace = CmdStream::WriteSetShPairs<ShaderCompute>(&m_csRing[CsRingSet::FirstShIdx()],
                                                          CsRingSet::NumSh(),
                                                          pCmdSpace);

    return pCmdSpace;
}

} // namespace Gfx12
} // namespace Pal
