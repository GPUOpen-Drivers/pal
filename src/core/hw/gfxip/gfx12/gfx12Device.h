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

#pragma once

#include "core/image.h"
#include "core/queue.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/gfx12/gfx12Barrier.h"
#include "core/hw/gfxip/gfx12/gfx12CmdUtil.h"
#include "core/hw/gfxip/gfx12/gfx12ComputeCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12Metadata.h"
#include "core/hw/gfxip/gfx12/gfx12SettingsLoader.h"
#include "core/hw/gfxip/rpm/gfx12/gfx12RsrcProcMgr.h"
#include "core/hw/gfxip/gfx12/gfx12OcclusionQueryPool.h"

#include <atomic>

namespace Pal
{
namespace Gfx12
{

// Forward decls.
struct UniversalCmdBufferDeviceConfig;

// Enumerates the types of Shader Rings available.
enum class ShaderRingType : uint32
{
    ComputeScratch,       // Scratch Ring for compute pipelines
    SamplePos,            // Constant buffer storing the device-level palette of sample patterns used to implement the
                          // samplepos instruction
    GfxScratch,           // Scratch Ring for graphics pipelines
    TfBuffer,             // Tess factor buffer
    OffChipLds,           // Off-Chip Tessellation LDS buffers
    PayloadData,          // Task -> GFX payload data
    MeshScratch,          // Mesh shader scratch ring
    TaskMeshCtrlDrawRing, // Task/Mesh shader control buffer ring and draw data ring
    VertexAttributes,     // Ring for passing vertex and primitive attributes from the HW GS to the PS
#if PAL_BUILD_GFX12
    PrimBuffer,           // Primitive ring buffer for primitive exports from the HW GS.
    PosBuffer,            // Position ring buffer for position exports from the HW GS.
#endif
    NumUniversal,         // Number of Rings in a RingSet associated with a universal Queue
    NumCompute = (SamplePos + 1), // Number of Rings in a RingSet associated with a compute Queue
};

// Contains the largest required item-size for each Shader Ring. Note that there is one item size tracker for each ring
// in a Universal Queue's RingSet.
struct ShaderRingItemSizes
{
    size_t itemSize[static_cast<size_t>(ShaderRingType::NumUniversal)];
};

// =====================================================================================================================
// Computes shader vgpr field. This field controls the number of VGPRs allocated by the hardware in granularity 4/8 for
// Wave64/Wave32, respectively.
static uint32 CalcNumVgprs(
    uint32 vgprCount,
    bool   isWave32,
    bool   dVgprEn = false)
{
    uint32 numVgprChunks = 0;
    if (dVgprEn == true)
    {
        // If dynamic VGPR is enabled, rsrc1.vpgrs must be set to 1
        numVgprChunks = 1;
    }
    else if (vgprCount == 0)
    {
        numVgprChunks = 0;
    }
    else
    {
        numVgprChunks = (vgprCount - 1) / ((isWave32) ? 8 : 4);
    }

    return numVgprChunks;
}

// =====================================================================================================================
template <uint32 N>
static constexpr uint32 GetRegIndex(const uint32 (&regList)[N], uint32 regOffset)
{
    uint32 index = UINT32_MAX;
    for (uint32 i = 0; i < N; i++)
    {
        if (regList[i] == regOffset)
        {
            index = i;
            break;
        }
    }

    return index;
}

class Device : public Pal::GfxDevice
{
public:
    explicit Device(Pal::Device* pDevice);
    virtual ~Device();

    virtual Result EarlyInit() override;
    virtual Result LateInit() override;

    static void PAL_STDCALL CreateTypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);
    static void PAL_STDCALL CreateUntypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);
    static void PAL_STDCALL CreateImageViewSrds(
        const IDevice*       pDevice,
        uint32               count,
        const ImageViewInfo* pImgViewInfo,
        void*                pOut);
    static void PAL_STDCALL CreateSamplerSrds(
        const IDevice*     pDevice,
        uint32             count,
        const SamplerInfo* pSamplerInfo,
        void*              pOut);
    static void PAL_STDCALL CreateBvhSrds(
        const IDevice* pDevice,
        uint32         count,
        const BvhInfo* pBvhInfo,
        void*          pOut);

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) override;

    virtual Result InitSettings() const override
    {
        PAL_ASSERT(m_pDdSettingsLoader != nullptr);
        return static_cast<Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->Init();
    }

    virtual Util::MetroHash::Hash GetSettingsHash() const override
    {
        Util::MetroHash::Hash zeroHash = {};
        return (m_pDdSettingsLoader != nullptr) ?
            static_cast<Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->GetSettingsHash() : zeroHash;
    }

    virtual void HwlValidateSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->ValidateSettings(pSettings);
    }

    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->OverrideDefaults(pSettings);
    }

    virtual void HwlRereadSettings() override {}

    virtual void HwlReadSettings() override
    {
        static_cast<Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->ReadSettings();
    }

    const Gfx12PalSettings& Settings() const
    {
        return static_cast<const Pal::Gfx12::SettingsLoader*>(m_pDdSettingsLoader)->GetSettings();
    }

    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const override;

    virtual Result GetLinearImageAlignments(
        LinearImageAlignments* pAlignments) const override;

    virtual size_t GetGfxQueueRingBufferSize() const override;

    virtual Result CreateGfxQueueRingBuffer(
        void* pPlacementAddr,
        GfxQueueRingBuffer** ppGfxQueueRb,
        GfxQueueRingBufferCreateInfo* pGfxQueueRingBufferCreateInfo) override;

    virtual size_t GetQueueContextSize(
        const QueueCreateInfo& createInfo) const override;
    virtual Result CreateQueueContext(
        const QueueCreateInfo& createInfo,
        Engine*                pEngine,
        void*                  pPlacementAddr,
        Pal::QueueContext**    ppQueueContext) override;

    virtual Result CreateDummyCommandStream(
        EngineType       engineType,
        Pal::CmdStream** ppCmdStream) const override;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;

    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        bool                             isInternal,
        IPipeline**                      ppPipeline) override;

    virtual size_t GetShaderLibrarySize(
        const ShaderLibraryCreateInfo& createInfo,
        Result*                        pResult) const override;

    virtual Result CreateShaderLibrary(
        const ShaderLibraryCreateInfo& createInfo,
        void*                          pPlacementAddr,
        bool                           isInternal,
        IShaderLibrary**               ppPipeline) override;

    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        bool                              isInternal,
        Result*                           pResult) const override;
    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo&         createInfo,
        const GraphicsPipelineInternalCreateInfo& internalInfo,
        void*                                     pPlacementAddr,
        bool                                      isInternal,
        IPipeline**                               ppPipeline) override;

    virtual size_t GetColorBlendStateSize() const override;
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const override;

    virtual size_t GetDepthStencilStateSize() const override;
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const override;

    virtual size_t GetMsaaStateSize() const override;
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const override;

    virtual size_t GetImageSize(
        const ImageCreateInfo& createInfo) const override;
    virtual bool ImagePrefersCloneCopy(const ImageCreateInfo& createInfo) const override;
    virtual void CreateImage(
        Pal::Image* pParentImage,
        ImageInfo*  pImageInfo,
        void*       pPlacementAddr,
        GfxImage**  ppImage) const override;

    virtual size_t GetBorderColorPaletteSize(
        const BorderColorPaletteCreateInfo& createInfo,
        Result*                             pResult) const override;

    virtual Result CreateBorderColorPalette(
        const BorderColorPaletteCreateInfo& createInfo,
        void*                               pPlacementAddr,
        IBorderColorPalette**               ppBorderColorPalette) const override;

    virtual size_t GetQueryPoolSize(const QueryPoolCreateInfo& createInfo, Result* pResult) const override;

    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const override;

    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo) const override;

    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        Pal::CmdBuffer**           ppCmdBuffer) override;

    virtual size_t GetIndirectCmdGeneratorSize(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        Result*                               pResult) const override;

    virtual Result CreateIndirectCmdGenerator(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        void*                                 pPlacementAddr,
        IIndirectCmdGenerator**               ppGenerator) const override;

    virtual size_t GetColorTargetViewSize(
        Result* pResult) const override;
    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        void*                             pPlacementAddr,
        IColorTargetView**                ppColorTargetView) const override;

    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const override;
    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        void*                                     pPlacementAddr,
        IDepthStencilView**                       ppDepthStencilView) const override;

    virtual size_t GetPerfExperimentSize(
        const PerfExperimentCreateInfo& createInfo,
        Result*                         pResult) const override;

    virtual ClearMethod GetDefaultSlowClearMethod(
        const ImageCreateInfo& createInfo,
        const SwizzledFormat&  clearFormat) const override;

    virtual void DisableImageViewSrdEdgeClamp(uint32 count, void* pImageSrds) const override;

    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const override;

    virtual Result CreateCmdUploadRingInternal(
        const CmdUploadRingCreateInfo& createInfo,
        Pal::CmdUploadRing**           ppCmdUploadRing) override;

    const CmdUtil& CmdUtil() const { return m_cmdUtil; }

    virtual Result SetSamplePatternPalette(
        const SamplePatternPalette& palette) override;
    void GetSamplePatternPalette(SamplePatternPalette* pSamplePatternPalette);

    PM4_PFP_CONTEXT_CONTROL GetContextControl() const;

    gpusize PrimBufferTotalMemSize() const;
    gpusize PosBufferTotalMemSize() const;
    uint32  GeomExportBufferMemSize(gpusize totalMemSize) const;

    const BoundGpuMemory& VertexAttributesMem(bool isTmz) const { return m_vertexAttributesMem[isTmz]; }
    const BoundGpuMemory& PrimBufferMem(bool isTmz)       const { return m_primBufferMem[isTmz];       }
    const BoundGpuMemory& PosBufferMem(bool isTmz)        const { return m_posBufferMem[isTmz];        }

    void CreateHiSZViewSrds(
        const Image&          image,
        const SubresRange&    subresRange,
        const SwizzledFormat& viewFormat,
        HiSZType              hiSZType,
        void*                 pOut) const;

    // Gets the source memory object used to accelerate occlusion query resets via ResetOcclusionQueryPool packet.
    const BoundGpuMemory& OcclusionResetMem() const { return m_occlusionResetSrcMem; }

    // Gets a copy of the reset value for a single occlusion query slot. The caller is responsible for determining the
    // size of the slot so that they do not read past the end of this buffer.
    const uint32* OcclusionSlotResetValue() const
        { return reinterpret_cast<const uint32*>(m_occlusionSlotResetValues); }

    const Gfx12::RsrcProcMgr& RsrcProcMgr() const { return m_rsrcProcMgr; }

    const Gfx12::BarrierMgr& BarrierMgr() const { return m_barrierMgr; }

    static void PAL_STDCALL DecodeBufferViewSrd(
        const IDevice*  pDevice,
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo);

    static void PAL_STDCALL DecodeImageViewSrd(
        const IDevice*   pDevice,
        const IImage*    pImage,
        const void*      pImageViewSrd,
        DecodedImageSrd* pDecodedInfo);

    const CompressionMode GetImageViewCompressionMode(
        CompressionMode  viewCompressionMode,
        CompressionMode  imageCompressionMode,
        const GpuMemory* pGpuMem) const;

    virtual bool CompressGpuMemory(
        const GpuMemoryCreateInfo& gpuMemCreateInfo,
        bool                       isCpuVisible,
        bool                       isClient) const override;

    regGB_ADDR_CONFIG GetGbAddrConfig() const;

#if DEBUG
    uint32* TemporarilyHangTheGpu(EngineType engineType, uint32 number, uint32* pCmdSpace) const override;
#endif

    Result AllocateVertexAttributesMem(bool isTmz);

    Result AllocatePrimBufferMem(bool isTmz);

    Result AllocatePosBufferMem(bool isTmz);

    bool EnableReleaseMemWaitCpDma() const { return Settings().enableReleaseMemWaitCpDma; }

    uint32 GetShaderPrefetchSize(gpusize shaderSizeBytes) const;

protected:
    virtual Result Finalize() override;
    virtual Result Cleanup() override;

private:
    void InitUniversalCmdBufferDeviceConfig(UniversalCmdBufferDeviceConfig* pDeviceConfig);
    void InitComputeCmdBufferDeviceConfig(  ComputeCmdBufferDeviceConfig*   pDeviceConfig);

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const override;

    virtual DccFormatEncoding ComputeDccFormatEncoding(
        const SwizzledFormat& swizzledFormat,
        const SwizzledFormat* pViewFormats,
        uint32                viewFormatCount) const override
    {
        return DccFormatEncoding::Incompatible;
    }

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const override;

    Result AllocateVertexOutputMem();

    Result InitOcclusionResetMem();

    Gfx12::CmdUtil     m_cmdUtil;
    Gfx12::BarrierMgr  m_barrierMgr;
    Gfx12::RsrcProcMgr m_rsrcProcMgr;

    // Tracks the sample pattern palette for sample pos shader ring. Access to this object must be
    // serialized using m_ringSizesLock.
    volatile SamplePatternPalette m_samplePatternPalette;

    // 0 - Non-TMZ, 1 - TMZ
    BoundGpuMemory     m_vertexAttributesMem[2];
    BoundGpuMemory     m_primBufferMem[2];
    BoundGpuMemory     m_posBufferMem[2];

    Util::Mutex        m_vertexOutputMutex; // Mutex guarding access to vertex output memory

    // Used as a source for ResetOcclusionQueryPool Gfx12 packet. This is used when RB harvesting disallows a 64-bit
    // DMA-fill.
    BoundGpuMemory     m_occlusionResetSrcMem;

    // An image of reset values for an entire occlusion query slot
    OcclusionQueryResultPair m_occlusionSlotResetValues[MaxNumRbs];

    mutable std::atomic<uint32> m_nextColorTargetViewId;
    mutable std::atomic<uint32> m_nextDepthStencilViewId;

};

// =====================================================================================================================
// Helper function to get gfx12 pal settings
inline const Gfx12PalSettings& GetGfx12Settings(const Pal::Device* device)
{
    return static_cast<Pal::Gfx12::Device*>(device->GetGfxDevice())->Settings();
}

} // Gfx12
} // Pal
