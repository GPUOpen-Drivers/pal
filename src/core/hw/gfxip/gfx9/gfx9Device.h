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

#pragma once

#include "core/device.h"
#include "core/hw/gfxip/gfx9/g_gfx9PalSettings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9MetaEq.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.h"

#include "palPipelineAbi.h"

namespace Pal
{

class  GfxCmdBuffer;
struct BarrierInfo;

namespace Gfx9
{

// This value is the result Log2(MaxMsaaRasterizerSamples) + 1.
constexpr uint32 MsaaLevelCount = 5;

// These types are used by CmdBarrier and its helpers to track which type of synchronization operations should be
// issued during the next call to IssueSyncs().
enum CacheSyncFlags : uint32
{
    CacheSyncInvSqI$     = 0x0001, // Invalidate the SQ instruction cache.
    CacheSyncInvSqK$     = 0x0002, // Invalidate the SQ scalar cache.
    CacheSyncFlushSqK$   = 0x0004, // Flush the SQ scalar cache.
    CacheSyncInvTcp      = 0x0008, // Invalidate L1 vector cache.
    CacheSyncInvTcc      = 0x0010, // Invalidate L2 cache.
    CacheSyncFlushTcc    = 0x0020, // Flush L2 cache.
    CacheSyncInvTccMd    = 0x0040, // Invalid TCC's meta-data cache.
    CacheSyncInvCbData   = 0x0080, // Invalidate CB data cache.
    CacheSyncInvCbMd     = 0x0100, // Invalidate CB meta-data cache.
    CacheSyncFlushCbData = 0x0200, // Flush CB data cache.
    CacheSyncFlushCbMd   = 0x0400, // Flush CB meta-data cache.
    CacheSyncInvDbData   = 0x0800, // Invalidate DB data cache.
    CacheSyncInvDbMd     = 0x1000, // Invalidate DB meta-data cache.
    CacheSyncFlushDbData = 0x2000, // Flush DB data cache.
    CacheSyncFlushDbMd   = 0x4000, // Flush DB meta-data cache.

    // Some helper masks to flush and invalidate various combinations of the back-end caches.
    CacheSyncFlushAndInvCbData = CacheSyncInvCbData         | CacheSyncFlushCbData,
    CacheSyncFlushAndInvCbMd   = CacheSyncInvCbMd           | CacheSyncFlushCbMd,
    CacheSyncFlushAndInvCb     = CacheSyncFlushAndInvCbData | CacheSyncFlushAndInvCbMd,
    CacheSyncFlushAndInvDbData = CacheSyncInvDbData         | CacheSyncFlushDbData,
    CacheSyncFlushAndInvDbMd   = CacheSyncInvDbMd           | CacheSyncFlushDbMd,
    CacheSyncFlushAndInvDb     = CacheSyncFlushAndInvDbData | CacheSyncFlushAndInvDbMd,
    CacheSyncFlushAndInvRb     = CacheSyncFlushAndInvCb     | CacheSyncFlushAndInvDb,
};

enum RegisterRangeType : uint32
{
    RegRangeUserConfig  = 0x0,
    RegRangeContext     = 0x1,
    RegRangeSh          = 0x2,
    RegRangeCsSh        = 0x3,
    RegRangeNonShadowed = 0x4,
};

struct SyncReqs
{
    uint32              cacheFlags;    // The set of CacheSyncFlags which must be done.
    regCP_ME_COHER_CNTL cpMeCoherCntl; // The cache operations only need to wait for these back-end resources.

    struct
    {
        uint32 waitOnEopTs    :  1;
        uint32 vsPartialFlush :  1;
        uint32 psPartialFlush :  1;
        uint32 csPartialFlush :  1;
        uint32 pfpSyncMe      :  1;
        uint32 syncCpDma      :  1;
        uint32 reserved       : 26;
    };
};

// PAL needs to reserve enough CE RAM space for the stream-out SRD table and for the user-data spill table for each
// pipeline bind point. Client CE RAM will be allocated after and CE load command needs a start alignment of 32 bytes,
// so PAL CE RAM needs to be multiple of 32 bytes to make sure loading only client CE RAM can be correctly done.
constexpr size_t ReservedCeRamBytes =
    ((sizeof(BufferSrd) * MaxStreamOutTargets)                                             +
     (sizeof(uint32) * static_cast<uint32>(PipelineBindPoint::Count) * MaxUserDataEntries) +
     ((sizeof(Util::Abi::PrimShaderCbLayout) + 255u) & ~255u)                              +
     (31)) & ~31;
constexpr size_t ReservedCeRamDwords = (ReservedCeRamBytes / sizeof(uint32));

// Minimum microcode feature version required by gfx-9 hardware to support IT_LOAD_SH/CONTEXT_INDEX packets.
constexpr uint32 MinUcodeFeatureVersionForLoadRegIndex = 29;

// Forward decl
static const Gfx9PalSettings& GetGfx9Settings(const Pal::Device& device);

// =====================================================================================================================
// GFX9 hardware layer implementation of GfxDevice. Responsible for creating HW-specific objects such as Queue contexts
// and owning child objects such as the SC manager.
class Device : public GfxDevice
{
public:
    explicit Device(Pal::Device* pDevice);
    virtual ~Device() { }

    virtual Result EarlyInit() override;
    virtual Result LateInit() override;
    virtual Result Finalize() override;
    virtual Result Cleanup() override;

    virtual void HwlValidateSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pSettingsLoader)->ValidateSettings(pSettings);
    }

    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pSettingsLoader)->OverrideDefaults(pSettings);
    }

    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const override;

    virtual Result GetLinearImageAlignments(LinearImageAlignments* pAlignments) const override;

    virtual void BindTrapHandler(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override;
    virtual void BindTrapBuffer(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override;

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) override;

    virtual size_t GetQueueContextSize(const QueueCreateInfo& createInfo) const override;

    virtual Result CreateQueueContext(
        Queue*         pQueue,
        Engine*        pEngine,
        void*          pPlacementAddr,
        QueueContext** ppQueueContext) override;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        bool                             isInternal,
        IPipeline**                      ppPipeline) override;

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

    virtual bool DetermineHwStereoRenderingSupported(
        const GraphicPipelineViewInstancingInfo& viewInstancingInfo) const override;

    virtual size_t GetColorBlendStateSize(const ColorBlendStateCreateInfo& createInfo, Result* pResult) const override;
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const override;

    virtual size_t GetDepthStencilStateSize(
        const DepthStencilStateCreateInfo& createInfo,
        Result*                            pResult) const override;
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const override;

    virtual size_t GetMsaaStateSize(
        const MsaaStateCreateInfo& createInfo,
        Result*                    pResult) const override;
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const override;
    virtual size_t GetImageSize(const ImageCreateInfo& createInfo) const override;
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

    virtual size_t GetQueryPoolSize(
        const QueryPoolCreateInfo& createInfo,
        Result*                    pResult) const override;
    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const override;

    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo) const override;
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) override;

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
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo,
        void*                                    pPlacementAddr,
        IColorTargetView**                       ppColorTargetView) const override;

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

    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const override;

    virtual Result CreateCmdUploadRingInternal(
        const CmdUploadRingCreateInfo& createInfo,
        Pal::CmdUploadRing**           ppCmdUploadRing) override;

    const CmdUtil& CmdUtil() const { return m_cmdUtil; }
    const Gfx9::RsrcProcMgr& RsrcProcMgr() const { return static_cast<Gfx9::RsrcProcMgr&>(*m_pRsrcProcMgr); }

    const Gfx9PalSettings& Settings() const
    {
        return static_cast<const Pal::Gfx9::SettingsLoader*>(m_pSettingsLoader)->GetSettings();
    }

    static uint32 CalcNumRecords(
        size_t      sizeInBytes,
        uint32      stride);

    // Gets the memory object used to accelerate occlusion query resets.
    const BoundGpuMemory& OcclusionResetMem() const { return m_occlusionSrcMem; }

    // Suballocated memory large enough to hold the output of a ZPASS_DONE event. It is only bound if the workaround
    // that requires it is enabled.
    const BoundGpuMemory& DummyZpassDoneMem() const { return m_dummyZpassDoneMem; }

    uint16 GetBaseUserDataReg(HwShaderStage  shaderStage) const;

    uint16  GetFirstUserDataReg(HwShaderStage  shaderStage) const
        { return m_firstUserDataReg[static_cast<uint32>(shaderStage)]; }

    // Gets a copy of the reset value for a single occlusion query slot. The caller is responsible for determining the
    // size of the slot so that they do not read past the end of this buffer.
    const uint32* OcclusionSlotResetValue() const
        { return reinterpret_cast<const uint32*>(m_occlusionSlotResetValues); }

    void   UpdateLargestRingSizes(const ShaderRingItemSizes* pRingSizesNeeded);
    void   GetLargestRingSizes(ShaderRingItemSizes* pRingSizesNeeded);
    uint32 QueueContextUpdateCounter() const { return m_queueContextUpdateCounter; }

    virtual Result SetSamplePatternPalette(const SamplePatternPalette& palette) override;
    void GetSamplePatternPalette(SamplePatternPalette* pSamplePatternPalette);

    virtual uint32 GetValidFormatFeatureFlags(
        const ChNumFormat format,
        const ImageAspect aspect,
        const ImageTiling tiling) const override;

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const override;

    virtual bool IsImageFormatOverrideNeeded(
        const ImageCreateInfo& imageCreateInfo,
        ChNumFormat*           pFormat,
        uint32*                pPixelsPerBlock) const override;

    virtual DccFormatEncoding ComputeDccFormatEncoding(const ImageCreateInfo& imageCreateInfo) const override;

    // Function definition for creating typed buffer view SRDs.
    static void PAL_STDCALL Gfx9CreateTypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating untyped buffer view SRDs.
    static void PAL_STDCALL Gfx9CreateUntypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating image view SRDs.
    static void PAL_STDCALL Gfx9CreateImageViewSrds(
        const IDevice*       pDevice,
        uint32               count,
        const ImageViewInfo* pImgViewInfo,
        void*                pOut);

    // Function definition for creating fmask view SRDs.
    static void PAL_STDCALL CreateFmaskViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const FmaskViewInfo*  pFmaskViewInfo,
        void*                 pOut);

    void CreateFmaskViewSrdsInternal(
        uint32                       count,
        const FmaskViewInfo*         pFmaskViewInfo,
        const FmaskViewInternalInfo* pFmaskViewInternalInfo,
        void*                        pOut) const;

    // Function definition for creating a sampler SRD.
    static void PAL_STDCALL Gfx9CreateSamplerSrds(
        const IDevice*      pDevice,
        uint32              count,
        const SamplerInfo*  pSamplerInfo,
        void*               pOut);

    void Barrier(GfxCmdBuffer* pCmdBuf, CmdStream* pCmdStream, const BarrierInfo& barrier) const;

    void FillCacheOperations(const SyncReqs& syncReqs, Developer::BarrierOperations* pOperations) const;

    void IssueSyncs(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        SyncReqs                      syncReqs,
        HwPipePoint                   waitPoint,
        gpusize                       rangeStartAddr,
        gpusize                       rangeSize,
        Developer::BarrierOperations* pOperations) const;
    void ExpandColor(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierTransition&      transition,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void TransitionDepthStencil(
        GfxCmdBuffer*                 pCmdBuf,
        GfxCmdBufferState             cmdBufState,
        const BarrierTransition&      transition,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void DescribeBarrier(
        GfxCmdBuffer*                 pCmdBuf,
        const BarrierTransition*      pTransition,
        Developer::BarrierOperations* pOperations) const;
    void DescribeBarrierStart(GfxCmdBuffer* pCmdBuf, uint32 reason) const;
    void DescribeBarrierEnd(GfxCmdBuffer* pCmdBuf, Developer::BarrierOperations* pOperations) const;

    uint32 GetMaxFragsLog2() const         { return GetGbAddrConfig().bits.MAX_COMPRESSED_FRAGS; }
    uint32 GetNumPipesLog2() const         { return GetGbAddrConfig().bits.NUM_PIPES; }
    uint32 GetNumShaderEnginesLog2() const { return GetGbAddrConfig().bits.NUM_SHADER_ENGINES; }
    uint32 GetNumRbsPerSeLog2() const      { return GetGbAddrConfig().bits.NUM_RB_PER_SE; }

    uint32 GetPipeInterleaveLog2() const;

    uint32 GetDbDfsmControl() const;

    const BoundGpuMemory& TrapHandler(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapHandler : m_computeTrapHandler; }
    const BoundGpuMemory& TrapBuffer(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapBuffer : m_computeTrapBuffer; }

    static uint32 GetBinSizeEnum(uint32  binSize);

    uint32 ComputeNoTessPrimGroupSize(uint32 targetPrimGroupSize) const;
    uint32 ComputeNoTessPatchPrimGroupSize(uint32 patchControlPoints) const;
    uint32 ComputeTessPrimGroupSize(uint32 numPatchesPerThreadGroup) const;

#if DEBUG
    uint32* TemporarilyHangTheGpu(uint32 number, uint32* pCmdSpace) const override;
#endif

    gpusize GetBaseAddress(const BufferSrd*  pBufferSrd) const;
    void    SetBaseAddress(BufferSrd*  pSrd, gpusize  baseAddress) const;
    void    InitBufferSrd(BufferSrd*  pBufferSrd, gpusize  gpuVirtAddr, gpusize stride) const;
    void    SetNumRecords(BufferSrd*  pSrd,  gpusize  numRecords) const;

    ColorFormat   GetHwColorFmt(SwizzledFormat  format) const;
    StencilFormat GetHwStencilFmt(ChNumFormat  format) const;
    ZFormat       GetHwZFmt(ChNumFormat  format) const;

    const RegisterRange*  GetRegisterRange(
        RegisterRangeType  rangeType,
        uint32*            pRangeEntries) const;

    PM4PFP_CONTEXT_CONTROL GetContextControl() const;

    virtual Result P2pBltWaModifyRegionListMemory(
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryCopyRegion*      pRegions,
        uint32*                      pNewRegionCount,
        MemoryCopyRegion*            pNewRegions,
        gpusize*                     pChunkAddrs) const override;

    virtual Result P2pBltWaModifyRegionListImage(
        const Pal::Image&            srcImage,
        const Pal::Image&            dstImage,
        uint32                       regionCount,
        const ImageCopyRegion*       pRegions,
        uint32*                      pNewRegionCount,
        ImageCopyRegion*             pNewRegions,
        gpusize*                     pChunkAddrs) const override;

    virtual Result P2pBltWaModifyRegionListImageToMemory(
        const Pal::Image&            srcImage,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const override;

    virtual Result P2pBltWaModifyRegionListMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const Pal::Image&            dstImage,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const override;

    virtual bool UsesIndexedLoad() const override
    {
        // Indexed load should always be used unless we're on Gfx9 and have incompatible microcode.
        return ((Parent()->ChipProperties().gfxLevel == GfxIpLevel::GfxIp9) &&
                (Parent()->EngineProperties().cpUcodeVersion < MinUcodeFeatureVersionForLoadRegIndex)) ? false : true;
    }

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const override;

private:
    Result InitOcclusionResetMem();
    const regGB_ADDR_CONFIG& GetGbAddrConfig() const;

    void Gfx9CreateFmaskViewSrdsInternal(
        const FmaskViewInfo&         viewInfo,
        const FmaskViewInternalInfo* pFmaskViewInternalInfo,
        Gfx9ImageSrd*                pSrd) const;

    void SetupWorkarounds();

    Gfx9::CmdUtil  m_cmdUtil;
    BoundGpuMemory m_occlusionSrcMem;   // If occlusionQueryDmaBufferSlots is in use, this is the source memory.
    BoundGpuMemory m_dummyZpassDoneMem; // A GFX9 workaround requires dummy ZPASS_DONE events which write to memory.

    // Tracks the largest item-size requirements for each type of Shader Ring. Access to this object must be serialized
    // using m_ringSizesLock.
    volatile ShaderRingItemSizes  m_largestRingSizes;
    Util::Mutex                   m_ringSizesLock;

    // Keep a watermark for the number of updates to the queue context. When a QueueContext pre-processes a submit, it
    // will check its watermark against the one owned by the device and update accordingly.
    volatile uint32               m_queueContextUpdateCounter;

    // Tracks the sample pattern palette for sample pos shader ring. Access to this object must be
    // serialized using m_samplePatternLock.
    volatile SamplePatternPalette m_samplePatternPalette;

    // An image of reset values for an entire occlusion query slot
    OcclusionQueryResultPair m_occlusionSlotResetValues[MaxNumRbs];

    // Store GPU memory and offsets for compute/graphics trap handlers and trap buffers.  Trap handlers are client-
    // installed hardware shaders that can be executed based on exceptions occurring in the main shader or in other
    // situations like supporting a debugger.  Trap buffers are just scratch memory that can be accessed from a trap
    // handler.  GFX9 has only one trap handler/buffer per VMID not per pipeline like GFX6 had.
    BoundGpuMemory m_computeTrapHandler;
    BoundGpuMemory m_computeTrapBuffer;
    BoundGpuMemory m_graphicsTrapHandler;
    BoundGpuMemory m_graphicsTrapBuffer;

    // Local copy of the GB_ADDR_CONFIG register
    const uint32      m_gbAddrConfig;
    const GfxIpLevel  m_gfxIpLevel;

    uint16         m_firstUserDataReg[HwShaderStage::Last];

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

static const Gfx9PalSettings& GetGfx9Settings(const Pal::Device& device)
{
    return static_cast<const Pal::Gfx9::Device*>(device.GetGfxDevice())->Settings();
}

} // Gfx9
} // Pal
