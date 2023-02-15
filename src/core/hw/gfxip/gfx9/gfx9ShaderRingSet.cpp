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

#include "core/platform.h"
#include "core/queue.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRing.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"

#include "palVectorImpl.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
ShaderRingSet::ShaderRingSet(
    Device* pDevice,
    size_t  numRings,      // Number of shader rings contained in this ring-set
    size_t  numSrds,       // Number of SRD's in the ring-set's table
    bool    isTmz)         // Is this shader ring TMZ protected
    :
    m_pDevice(pDevice),
    m_numRings(numRings),
    m_numSrds(numSrds),
    m_tmzEnabled(isTmz),
    m_ppRings(nullptr),
    m_pSrdTable(nullptr),
    m_gfxLevel(m_pDevice->Parent()->ChipProperties().gfxLevel),
    m_deferredFreeMemList(pDevice->GetPlatform()),
    m_freedItemCount(0)
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
    srdMemCreateInfo.heaps[0]  = GpuHeapLocal;
    srdMemCreateInfo.heaps[1]  = GpuHeapGartUswc;
    srdMemCreateInfo.heaps[2]  = GpuHeapGartCacheable;
    srdMemCreateInfo.heapCount = 3;

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
                    PAL_NEW(ScratchRing,
                            m_pDevice->GetPlatform(),
                            AllocObject)(m_pDevice, m_pSrdTable, ShaderCompute, m_tmzEnabled);
                break;

            case ShaderRingType::GfxScratch:
                m_ppRings[idx] =
                    PAL_NEW(ScratchRing,
                            m_pDevice->GetPlatform(),
                            AllocObject)(m_pDevice, m_pSrdTable, ShaderGraphics, m_tmzEnabled);
                break;

            case ShaderRingType::GsVs:
                m_ppRings[idx] = PAL_NEW(GsVsRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::TfBuffer:
                m_ppRings[idx] =
                    PAL_NEW(TessFactorBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::OffChipLds:
                m_ppRings[idx] =
                    PAL_NEW(OffchipLdsBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::SamplePos:
                m_ppRings[idx] =
                    PAL_NEW(SamplePosBuffer, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::MeshScratch:
                m_ppRings[idx] =
                    PAL_NEW(MeshScratchRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::PayloadData:
                m_ppRings[idx] =
                    PAL_NEW(PayloadDataRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;

            case ShaderRingType::TaskMeshCtrlDrawRing:
                m_ppRings[idx] =
                    PAL_NEW(TaskMeshCtrlDrawRing, m_pDevice->GetPlatform(), AllocObject)(m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;
#if PAL_BUILD_GFX11
            case ShaderRingType::VertexAttributes:
                m_ppRings[idx] =
                    PAL_NEW(VertexAttributeRing, m_pDevice->GetPlatform(), AllocObject)
                           (m_pDevice, m_pSrdTable, m_tmzEnabled);
                break;
#endif

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
            result = m_ppRings[ring]->Validate(ringSizes.itemSize[ring],
                                               &deferredMem);
            if (deferredMem.pGpuMemory != nullptr)
            {
                // If any shaderRing need to defer free ring memory,
                // the current shadertable map / unmap needs to be deferred also
                deferFreeSrdTable = true;
                m_deferredFreeMemList.PushBack(deferredMem);
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
        if (deferFreeSrdTable)
        {
            // save the current shardTable, since it might still be needed
            ShaderRingMemory ringMem = {m_srdTableMem.Memory(),
                                        m_srdTableMem.Offset(),
                                        lastTimeStamp};
            m_deferredFreeMemList.PushBack(ringMem);
            m_srdTableMem.Update(nullptr, 0);

            // Allocate a new shaderTable
            GpuMemoryCreateInfo srdMemCreateInfo = { };
            srdMemCreateInfo.size      = TotalMemSize();
            srdMemCreateInfo.priority  = GpuMemPriority::Normal;
            srdMemCreateInfo.vaRange   = VaRange::DescriptorTable;
            srdMemCreateInfo.heaps[0]  = GpuHeapLocal;
            srdMemCreateInfo.heaps[1]  = GpuHeapGartUswc;
            srdMemCreateInfo.heaps[2]  = GpuHeapGartCacheable;
            srdMemCreateInfo.heapCount = 3;

            GpuMemoryInternalCreateInfo internalInfo = { };
            internalInfo.flags.alwaysResident = 1;

            GpuMemory* pGpuMemory = nullptr;
            gpusize    memOffset  = 0;

            // Allocate the memory object for each ring-set's SRD table.
            result = m_pDevice->Parent()->MemMgr()->AllocateGpuMem(srdMemCreateInfo,
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
            }
        }

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
void ShaderRingSet::ClearDeferredFreeMemory(
    SubmissionContext* pSubmissionCtx)
{
    if (m_deferredFreeMemList.NumElements() > 0)
    {
        InternalMemMgr*const pMemMgr = m_pDevice->Parent()->MemMgr();

        for (uint32 i = 0; i < m_deferredFreeMemList.NumElements(); i++)
        {
            ShaderRingMemory pRingMem = m_deferredFreeMemList.At(i);
            if (pRingMem.pGpuMemory != nullptr)
            {
                if (pSubmissionCtx->IsTimestampRetired(pRingMem.timestamp))
                {
                    pMemMgr->FreeGpuMem(pRingMem.pGpuMemory, pRingMem.offset);
                    m_freedItemCount++;
                }
            }
        }

        if (m_freedItemCount == m_deferredFreeMemList.NumElements())
        {
            m_deferredFreeMemList.Clear();
            m_freedItemCount = 0;
        }
    }
}

// =====================================================================================================================
UniversalRingSet::UniversalRingSet(
    Device* pDevice,
    bool    isTmz)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumUniversal),
                  static_cast<size_t>(ShaderRingSrd::NumUniversal),
                  isTmz)
{
    memset(&m_regs, 0, sizeof(m_regs));
}

// Some registers were moved from user space to privileged space, we must access them using _UMD or _REMAP registers.
// The problem is that only some ASICs moved the registers so we can't use any one name consistently. The good news is
// that most of the _UMD and _REMAP registers have the same user space address as the old user space registers.
// If these asserts pass we can just use the Gfx09 version of these registers everywhere in our code.
static_assert(Gfx09::mmVGT_GSVS_RING_SIZE            == Gfx101::mmVGT_GSVS_RING_SIZE_UMD, "");
static_assert(NotGfx10::mmVGT_HS_OFFCHIP_PARAM       == Gfx101::mmVGT_HS_OFFCHIP_PARAM_UMD, "");
static_assert(NotGfx10::mmVGT_TF_MEMORY_BASE         == Gfx101::mmVGT_TF_MEMORY_BASE_UMD, "");
static_assert(NotGfx10::mmVGT_TF_RING_SIZE           == Gfx101::mmVGT_TF_RING_SIZE_UMD, "");
static_assert(Gfx09::mmVGT_GSVS_RING_SIZE            == Nv2x::mmVGT_GSVS_RING_SIZE_UMD, "");
static_assert(NotGfx10::mmVGT_HS_OFFCHIP_PARAM       == Nv2x::mmVGT_HS_OFFCHIP_PARAM_UMD, "");
static_assert(NotGfx10::mmVGT_TF_MEMORY_BASE         == Nv2x::mmVGT_TF_MEMORY_BASE_UMD, "");
static_assert(NotGfx10::mmVGT_TF_RING_SIZE           == Nv2x::mmVGT_TF_RING_SIZE_UMD, "");
static_assert(Gfx101::mmVGT_TF_MEMORY_BASE_HI_UMD    == Nv2x::mmVGT_TF_MEMORY_BASE_HI_UMD, "");
static_assert(Gfx09::mmVGT_GSVS_RING_SIZE            == Apu103::mmVGT_GSVS_RING_SIZE, "");
static_assert(NotGfx10::mmVGT_HS_OFFCHIP_PARAM       == Apu103::mmVGT_HS_OFFCHIP_PARAM, "");
static_assert(NotGfx10::mmVGT_TF_MEMORY_BASE         == Apu103::mmVGT_TF_MEMORY_BASE, "");
static_assert(NotGfx10::mmVGT_TF_RING_SIZE           == Apu103::mmVGT_TF_RING_SIZE, "");
static_assert(Gfx101::mmVGT_TF_MEMORY_BASE_HI_UMD    == Apu103::mmVGT_TF_MEMORY_BASE_HI, "");

// =====================================================================================================================
// Initializes this Universal-Queue shader-ring set object.
Result UniversalRingSet::Init()
{
    const Pal::Device&  device = *(m_pDevice->Parent());

    // First, call the base class' implementation to allocate and init each Ring object.
    Result result = ShaderRingSet::Init();

    if (result == Result::Success)
    {
        // Set up the SPI_TMPRING_SIZE for the graphics shader scratch ring.
        const ScratchRing*const pScratchRingGfx =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);

        m_regs.gfxScratchRingSize.bits.WAVES    = pScratchRingGfx->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(device))
        {
            m_regs.gfxScratchRingSize.gfx11.WAVESIZE = pScratchRingGfx->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.gfxScratchRingSize.gfx09_10.WAVESIZE = pScratchRingGfx->CalculateWaveSize();
        }

        // Set up the COMPUTE_TMPRING_SIZE for the compute shader scratch ring.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_regs.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(device))
        {
            m_regs.computeScratchRingSize.gfx11.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.computeScratchRingSize.gfx09_10.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }

        // The OFFCHIP_GRANULARITY field of VGT_HS_OFFCHIP_PRARM is determined at init-time by the value of the related
        // setting.
        if (IsGfx103PlusExclusive(device))
        {
            m_regs.vgtHsOffchipParam.gfx103PlusExclusive.OFFCHIP_GRANULARITY = m_pDevice->Settings().offchipLdsBufferSize;
        }
        else if (IsGfx9(device) || IsGfx101(device)
           )
        {
            m_regs.vgtHsOffchipParam.most.OFFCHIP_GRANULARITY = m_pDevice->Settings().offchipLdsBufferSize;
        }
        else
        {
            // Which GPU is this?
            PAL_ASSERT_ALWAYS();
        }

#if PAL_BUILD_GFX11
        m_regs.spiAttributeRingSize.bits.BIG_PAGE                 = 0;
        m_regs.spiAttributeRingSize.bits.L1_POLICY                = GL1_CACHE_POLICY_MISS_EVICT;
        m_regs.spiAttributeRingSize.bits.L2_POLICY                = GL2_CACHE_POLICY_LRU;
        m_regs.spiAttributeRingSize.bits.LLC_NOALLOC              = 0;
        m_regs.spiAttributeRingSize.bits.GL1_PERF_COUNTER_DISABLE = 0;
#endif
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
    const SamplePatternPalette& samplePatternPalette,
    uint64                      lastTimeStamp,
    uint32*                     pReallocatedRings)
{
    const Pal::Device&  device = *(m_pDevice->Parent());

    // Check if the TaskMesh control draw ring has already been initialized.
    const bool tsMsCtrlDrawInitialized =
        m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)]->IsMemoryValid();

    // First, perform the base class' validation.
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette, lastTimeStamp, pReallocatedRings);

    const bool drawDataReAlloc =
        Util::TestAnyFlagSet(*pReallocatedRings, (1 << static_cast<uint32>(ShaderRingType::TaskMeshCtrlDrawRing))) ||
        Util::TestAnyFlagSet(*pReallocatedRings, (1 << static_cast<uint32>(ShaderRingType::PayloadData)));

    // Initialize the task shader control buffer and draw ring after they have been allocated.
    // Also, if we re-allocate the draw and/or payload data rings, we must ensure that all task shader-related
    // rings are re-allocated at the same time and re-initialized.
    TaskMeshCtrlDrawRing* pTaskMeshCtrlDrawRing =
        static_cast<TaskMeshCtrlDrawRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)]);

    if (((tsMsCtrlDrawInitialized == false) || drawDataReAlloc) && pTaskMeshCtrlDrawRing->IsMemoryValid())
    {
        pTaskMeshCtrlDrawRing->InitializeControlBufferAndDrawRingBuffer();
    }

    if (result == Result::Success)
    {
        // Next, update our PM4 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingGfx  =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::GfxScratch)]);
        const ScratchRing*const pScratchRingCs   =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);
        const ShaderRing*const  pGsVsRing        = m_ppRings[static_cast<size_t>(ShaderRingType::GsVs)];
        const TessFactorBuffer*const  pTfBuffer  =
            static_cast<TessFactorBuffer*>(m_ppRings[static_cast<size_t>(ShaderRingType::TfBuffer)]);
        const OffchipLdsBuffer*const pOffchipLds =
            static_cast<OffchipLdsBuffer*>(m_ppRings[static_cast<size_t>(ShaderRingType::OffChipLds)]);

        // Scratch rings:
        m_regs.gfxScratchRingSize.bits.WAVES        = pScratchRingGfx->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(device))
        {
            m_regs.gfxScratchRingSize.gfx11.WAVESIZE     = pScratchRingGfx->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.gfxScratchRingSize.gfx09_10.WAVESIZE     = pScratchRingGfx->CalculateWaveSize();
        }

#if PAL_BUILD_GFX11
        if (pScratchRingGfx->IsMemoryValid())
        {
            m_regs.spiGfxScratchBaseLo.bits.DATA = Get256BAddrLo(pScratchRingGfx->GpuVirtAddr());
            m_regs.spiGfxScratchBaseHi.bits.DATA = Get256BAddrHi(pScratchRingGfx->GpuVirtAddr());
        }
#endif

        m_regs.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(device))
        {
            m_regs.computeScratchRingSize.gfx11.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.computeScratchRingSize.gfx09_10.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }

#if PAL_BUILD_GFX11
        if (pScratchRingCs->IsMemoryValid())
        {
            m_regs.computeDispatchScratchBaseLo.bits.DATA = Get256BAddrLo(pScratchRingCs->GpuVirtAddr());
            m_regs.computeDispatchScratchBaseHi.bits.DATA = Get256BAddrHi(pScratchRingCs->GpuVirtAddr());
        }
#endif

        // ES/GS and GS/VS ring size registers are in units of 64 DWORD's.
        constexpr uint32 GsRingSizeAlignmentShift = 6;

        m_regs.vgtGsVsRingSize.bits.MEM_SIZE = (pGsVsRing->MemorySizeDwords() >> GsRingSizeAlignmentShift);

        // Tess-Factor Buffer:
        if (pTfBuffer->IsMemoryValid())
        {
            const uint32  addrLo = Get256BAddrLo(pTfBuffer->GpuVirtAddr());
            const uint32  addrHi = Get256BAddrHi(pTfBuffer->GpuVirtAddr());

            m_regs.vgtTfMemoryBaseLo.bits.BASE    = addrLo;
            m_regs.vgtTfMemoryBaseHi.bits.BASE_HI = addrHi;

#if PAL_BUILD_GFX11
            if (IsGfx11(m_gfxLevel))
            {
                m_regs.vgtTfRingSize.gfx11.SIZE = pTfBuffer->TfRingSize();
            }
            else
#endif
            {
                m_regs.vgtTfRingSize.gfx09_10.SIZE = pTfBuffer->TfRingSize();
            }
        }

        // Off-chip LDS Buffers:
        if (pOffchipLds->IsMemoryValid())
        {
            const uint32 offchipBuffering = pOffchipLds->OffchipBuffering();

            if (IsGfx103PlusExclusive(device))
            {
                m_regs.vgtHsOffchipParam.gfx103PlusExclusive.OFFCHIP_BUFFERING = offchipBuffering;
            }
            else if (IsGfx9(device) || IsGfx10(m_gfxLevel)
               )
            {
                m_regs.vgtHsOffchipParam.most.OFFCHIP_BUFFERING = offchipBuffering;
            }

        }

#if PAL_BUILD_GFX11
        const ShaderRing*const pAttribThruMem = m_ppRings[static_cast<size_t>(ShaderRingType::VertexAttributes)];
        if (pAttribThruMem->IsMemoryValid())
        {
            // AttribThruMem addr and size registers are in units of 64KB
            constexpr uint32 AttribThruMemShift = 16;

            m_regs.spiAttributeRingBase.bits.BASE = pAttribThruMem->GpuVirtAddr() >> AttribThruMemShift;

            const uint32 numSes = m_pDevice->Parent()->ChipProperties().gfx9.numShaderEngines;

            // Size field is biased by 1. This the size per SE
            m_regs.spiAttributeRingSize.bits.MEM_SIZE =
                ((pAttribThruMem->MemorySizeBytes() / numSes) >> AttribThruMemShift) - 1;
        }
#endif
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
    const uint32 srdTableBaseLo = LowPart(m_srdTableMem.GpuVirtAddr());

    const CmdUtil& cmdUtil  = m_pDevice->CmdUtil();
    const auto&    regInfo  = cmdUtil.GetRegInfo();

    // Issue VS_PARTIAL_FLUSH and VGT_FLUSH events to make sure it is safe to write the ring config registers.
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(VS_PARTIAL_FLUSH, EngineTypeUniversal, pCmdSpace);
    pCmdSpace += cmdUtil.BuildNonSampleEventWrite(VGT_FLUSH, EngineTypeUniversal, pCmdSpace);

    if (m_gfxLevel == GfxIpLevel::GfxIp9)
    {
        pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(NotGfx10::mmVGT_TF_MEMORY_BASE,
                                                      Gfx09::mmVGT_TF_MEMORY_BASE_HI,
                                                      &m_regs.vgtTfMemoryBaseLo,
                                                      pCmdSpace);
    }
    else if (IsGfx10Plus(m_gfxLevel))
    {
        // The use of the "NotGfx10" namespace here is non-intuitive; for GFX10 parts, this is the same offset
        // as the mmVGT_TF_MEMORY_BASE_UMD register.
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(NotGfx10::mmVGT_TF_MEMORY_BASE,
                                                     m_regs.vgtTfMemoryBaseLo.u32All,
                                                     pCmdSpace);

        // Likewise, this isn't just a GFX10.1 register; this register exists (with and without the UMD extension)
        // on all GFX10+ parts.
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx101::mmVGT_TF_MEMORY_BASE_HI_UMD,
                                                     m_regs.vgtTfMemoryBaseHi.u32All,
                                                     pCmdSpace);
    }

    pCmdSpace = pCmdStream->WriteSetOneConfigReg(NotGfx10::mmVGT_TF_RING_SIZE, m_regs.vgtTfRingSize.u32All, pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneConfigReg(NotGfx10::mmVGT_HS_OFFCHIP_PARAM,
                                                 m_regs.vgtHsOffchipParam.u32All,
                                                 pCmdSpace);

    if (m_pDevice->Parent()->ChipProperties().gfxip.supportsHwVs)
    {
        pCmdSpace = pCmdStream->WriteSetOneConfigReg(Gfx09::mmVGT_GSVS_RING_SIZE,
                                                    m_regs.vgtGsVsRingSize.u32All,
                                                    pCmdSpace);
    }

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                                                           srdTableBaseLo,
                                                           pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_TMPRING_SIZE,
                                                            m_regs.computeScratchRingSize.u32All,
                                                            pCmdSpace);

    const uint32 gfxSrdTableGpuVaLo[] =
    {
        static_cast<uint32>(m_pDevice->GetBaseUserDataReg(HwShaderStage::Hs) + InternalTblStartReg),
        static_cast<uint32>(regInfo.mmUserDataStartGsShaderStage             + InternalTblStartReg),
        mmSPI_SHADER_USER_DATA_PS_0                                          + InternalTblStartReg,
        HasHwVs::mmSPI_SHADER_USER_DATA_VS_0                                 + InternalTblStartReg,
    };

    size_t gfxSrdRegLocsCount = m_pDevice->Parent()->ChipProperties().gfxip.supportsHwVs ?
                                ArrayLen(gfxSrdTableGpuVaLo) :
                                ArrayLen(gfxSrdTableGpuVaLo) - 1;

    for (uint32 s = 0; s < gfxSrdRegLocsCount; ++s)
    {
        pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderGraphics>(gfxSrdTableGpuVaLo[s], srdTableBaseLo, pCmdSpace);
    }

    const ShaderRing* const  pControlBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)];
    if (pControlBuffer->IsMemoryValid())
    {
        pCmdSpace += CmdUtil::BuildTaskStateInit(ShaderGraphics,
                                                 pControlBuffer->GpuVirtAddr(),
                                                 PredDisable,
                                                 pCmdSpace);
    }

#if PAL_BUILD_GFX11
    const ShaderRing* const pVertexAttributes = m_ppRings[static_cast<size_t>(ShaderRingType::VertexAttributes)];
    if (pVertexAttributes->IsMemoryValid())
    {
        // GPU page fault or application corruption is observed when ATM base address switch during HP3D <-> LP3D
        // transition. According to hardware folks, BOTTOM_OF_PIPE_TS will make sure that the attribute buffer has been
        // fully deallocated before the registers are updated. And PS_PARTIAL_FLUSH is not sufficient for changing ATM
        // regs.
        // Since PWS is enabled by default on GFX11, here we disregard the UsePws setting and add a PWS stall directly.
        // Otherwise we have to allocate a timestamp memory allocation which is never used in default path.
        pCmdSpace += cmdUtil.BuildWaitEopPws(HwPipePostPrefetch, SyncGlxNone, SyncRbNone, pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetSeqConfigRegs(Gfx11::mmSPI_ATTRIBUTE_RING_BASE,
                                                      Gfx11::mmSPI_ATTRIBUTE_RING_SIZE,
                                                      &m_regs.spiAttributeRingBase,
                                                      pCmdSpace);
    }

    if (IsGfx11(m_gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO,
                                                  mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI,
                                                  Pm4ShaderType::ShaderCompute,
                                                  &m_regs.computeDispatchScratchBaseLo,
                                                  pCmdSpace);

        pCmdSpace = pCmdStream->WriteSetSeqContextRegs(Gfx11::mmSPI_GFX_SCRATCH_BASE_LO,
                                                       Gfx11::mmSPI_GFX_SCRATCH_BASE_HI,
                                                       &m_regs.spiGfxScratchBaseLo,
                                                       pCmdSpace);
    }
#endif

    return pCmdStream->WriteSetOneContextReg(mmSPI_TMPRING_SIZE, m_regs.gfxScratchRingSize.u32All, pCmdSpace);
}

// =====================================================================================================================
// Writes the compute portion of this ShaderRingSet into the command stream provided. This is used by the ACE-GFX gang
// submit, where ACE commands are submitted together with GFX in the DE command stream.
uint32* UniversalRingSet::WriteComputeCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace) const
{
    PAL_ASSERT(pCmdSpace != nullptr);

    const CmdUtil& cmdUtil = m_pDevice->CmdUtil();

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                                                            LowPart(m_srdTableMem.GpuVirtAddr()),
                                                            pCmdSpace);

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_TMPRING_SIZE,
                                                            m_regs.computeScratchRingSize.u32All,
                                                            pCmdSpace);
    const ShaderRing* const  pControlBuffer = m_ppRings[static_cast<size_t>(ShaderRingType::TaskMeshCtrlDrawRing)];
    if (pControlBuffer->IsMemoryValid())
    {
        pCmdSpace += CmdUtil::BuildTaskStateInit(ShaderCompute,
                                                 pControlBuffer->GpuVirtAddr(),
                                                 PredDisable,
                                                 pCmdSpace);
    }

#if PAL_BUILD_GFX11
    if (IsGfx11(m_gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO,
                                                  mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI,
                                                  Pm4ShaderType::ShaderCompute,
                                                  &m_regs.computeDispatchScratchBaseLo,
                                                  pCmdSpace);
    }
#endif

    return pCmdSpace;
}

// =====================================================================================================================
ComputeRingSet::ComputeRingSet(
    Device* pDevice,
    bool    isTmz)
    :
    ShaderRingSet(pDevice,
                  static_cast<size_t>(ShaderRingType::NumCompute),
                  static_cast<size_t>(ShaderRingSrd::NumCompute),
                  isTmz)
{
    memset(&m_regs, 0, sizeof(m_regs));
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

        m_regs.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(*(m_pDevice->Parent())))
        {
            m_regs.computeScratchRingSize.gfx11.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.computeScratchRingSize.gfx09_10.WAVESIZE = pScratchRingCs->CalculateWaveSize();
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
    uint64                      lastTimeStamp,
    uint32*                     pReallocatedRings)
{
    // First, perform the base class' validation.
    Result result = ShaderRingSet::Validate(ringSizes, samplePatternPalette, lastTimeStamp, pReallocatedRings);

    if (result == Result::Success)
    {
        // Next, update our PM4 image with the register state reflecting the validated shader Rings.
        const ScratchRing*const pScratchRingCs =
            static_cast<ScratchRing*>(m_ppRings[static_cast<size_t>(ShaderRingType::ComputeScratch)]);

        m_regs.computeScratchRingSize.bits.WAVES    = pScratchRingCs->CalculateWaves();
#if PAL_BUILD_GFX11
        if (IsGfx11(*(m_pDevice->Parent())))
        {
            m_regs.computeScratchRingSize.gfx11.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }
        else
#endif
        {
            m_regs.computeScratchRingSize.gfx09_10.WAVESIZE = pScratchRingCs->CalculateWaveSize();
        }

#if PAL_BUILD_GFX11
        if (pScratchRingCs->IsMemoryValid())
        {
            m_regs.computeDispatchScratchBaseLo.bits.DATA = Get256BAddrLo(pScratchRingCs->GpuVirtAddr());
            m_regs.computeDispatchScratchBaseHi.bits.DATA = Get256BAddrHi(pScratchRingCs->GpuVirtAddr());
        }
#endif
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
    const uint32 srdTableBaseLo = LowPart(m_srdTableMem.GpuVirtAddr());

    pCmdSpace = pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_USER_DATA_0 + InternalTblStartReg,
                                                            srdTableBaseLo,
                                                            pCmdSpace);

#if PAL_BUILD_GFX11
    if (IsGfx11(m_gfxLevel))
    {
        pCmdSpace = pCmdStream->WriteSetSeqShRegs(mmCOMPUTE_DISPATCH_SCRATCH_BASE_LO,
                                                  mmCOMPUTE_DISPATCH_SCRATCH_BASE_HI,
                                                  Pm4ShaderType::ShaderCompute,
                                                  &m_regs.computeDispatchScratchBaseLo,
                                                  pCmdSpace);
    }
#endif

    return pCmdStream->WriteSetOneShReg<ShaderCompute>(mmCOMPUTE_TMPRING_SIZE,
                                                       m_regs.computeScratchRingSize.u32All,
                                                       pCmdSpace);
}

} // Gfx9
} // Pal
