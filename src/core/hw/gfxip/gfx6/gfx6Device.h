/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"
#include "core/hw/gfxip/gfx6/gfx6CmdUtil.h"
#include "core/hw/gfxip/gfx6/gfx6SettingsLoader.h"
#include "core/hw/gfxip/gfx6/gfx6ShaderRingSet.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/rpm/gfx6/gfx6RsrcProcMgr.h"

namespace Pal
{
namespace Gfx6
{

// Simple structure used by CmdBarrier and its helpers to track which type of synchronization operations should be
// issued during the next call to IssueSyncs().
struct SyncReqs
{
    regCP_COHER_CNTL cpCoherCntl;

    struct
    {
        uint32 waitOnEopTs      :  1;
        uint32 cacheFlushAndInv :  1;
        uint32 vsPartialFlush   :  1;
        uint32 psPartialFlush   :  1;
        uint32 csPartialFlush   :  1;
        uint32 pfpSyncMe        :  1;
        uint32 syncCpDma        :  1;
        uint32 reserved         : 25;
    };
};

// Enumeration controls the behavior of the Gfx8 TC Compatibility DB flush workaround.
enum Gfx8TcCompatDbFlushWorkaround : uint32
{
    Gfx8TcCompatDbFlushWaNever = 0x00000000,
    Gfx8TcCompatDbFlushWaNormal = 0x00000001,
    Gfx8TcCompatDbFlushWaAlways = 0x00000002,

};

// PAL needs to reserve enough CE RAM space for the stream-out SRD table and for the user-data spill table for each
// pipeline bind point.  Client CE RAM will be allocated after and CE load command needs a start alignment of 32 bytes,
// so PAL CE RAM needs to be multiple of 32 bytes to make sure loading only client CE RAM can be correctly done.
constexpr size_t ReservedCeRamBytes =
    ((sizeof(BufferSrd) * MaxStreamOutTargets) +
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 469
     (sizeof(BufferSrd) * MaxVertexBuffers) +
#endif
     (sizeof(uint32) * static_cast<uint32>(PipelineBindPoint::Count) * MaxUserDataEntries) +
     (31)) & ~31;
// so PAL CE RAM needs to be multiple of 32 bytes to make sure loading only client CE RAM can be correctly done.
constexpr size_t ReservedCeRamDwords = (ReservedCeRamBytes / sizeof(uint32));

// =====================================================================================================================
// GFX6 hardware layer implementation of GfxDevice. Responsible for creating HW-specific objects such as Queue contexts
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
        static_cast<Pal::Gfx6::SettingsLoader*>(m_pSettingsLoader)->ValidateSettings(pSettings);
    }

    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx6::SettingsLoader*>(m_pSettingsLoader)->OverrideDefaults(pSettings);
    }

    virtual void HwlRereadSettings() override
    {
        m_pSettingsLoader->RereadSettings();
    }

    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const override;

    virtual Result GetLinearImageAlignments(LinearImageAlignments* pAlignments) const override;

    virtual void BindTrapHandler(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override;
    virtual void BindTrapBuffer(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) override;

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) override;

    virtual Result CreateDummyCommandStream(EngineType engineType, Pal::CmdStream** ppCmdStream) const override;

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

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 556
   virtual size_t GetShaderLibrarySize(
        const ShaderLibraryCreateInfo&  createInfo,
        Result*                         pResult) const override;
    virtual Result CreateShaderLibrary(
        const ShaderLibraryCreateInfo&  createInfo,
        void*                           pPlacementAddr,
        bool                            isInternal,
        IShaderLibrary**                ppPipeline) override;
#endif

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

    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const override;

    virtual Result CreateCmdUploadRingInternal(
        const CmdUploadRingCreateInfo& createInfo,
        Pal::CmdUploadRing**           ppCmdUploadRing) override;

    const CmdUtil& CmdUtil() const { return m_cmdUtil; }
    const Gfx6::RsrcProcMgr& RsrcProcMgr() const { return m_rsrcProcMgr; }

    const Gfx6PalSettings& Settings() const
    {
        return static_cast<const Pal::Gfx6::SettingsLoader*>(m_pSettingsLoader)->GetSettings();
    }

    gpusize CalcNumRecords(gpusize range, gpusize stride) const;
    gpusize CalcBufferSrdRange(const BufferSrd& srd) const;

    // Gets the memory object used to accelerate occlusion query resets.
    const BoundGpuMemory& OcclusionResetMem() const { return m_occlusionSrcMem; }
    const BoundGpuMemory& CpDmaPatchMem() const { return m_cpDmaPatchMem; }

    // Gets a copy of the reset value for a single occlusion query slot. The caller is responsible for determining the
    // size of the slot so that they do not read past the end of this buffer.
    const uint32* OcclusionSlotResetValue() const
        { return reinterpret_cast<const uint32*>(m_occlusionSlotResetValues); }

    static gpusize CpDmaCompatAlignment(const Device& device, gpusize alignment);

    uint32* WriteGfx7LoadEsGsVsPm4Img(CmdStream* pCmdStream, uint32* pCmdSpace) const;

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
        uint32*                pPixelsPerBlock) const override { return false; }

    virtual DccFormatEncoding ComputeDccFormatEncoding(const ImageCreateInfo& imageCreateInfo) const override;

    // Function definition for creating typed buffer view SRDs.
    static void PAL_STDCALL CreateTypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating untyped buffer view SRDs.
    static void PAL_STDCALL CreateUntypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating image view SRDs.
    static void PAL_STDCALL CreateImageViewSrds(
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

    void CreateFmaskViewSrds(
        uint32                       count,
        const FmaskViewInfo*         pFmaskViewInfo,
        const FmaskViewInternalInfo* pFmaskViewInternalInfo,
        void*                        pOut) const;

    // Function definition for creating a sampler SRD.
    static void PAL_STDCALL CreateSamplerSrds(
        const IDevice*      pDevice,
        uint32              count,
        const SamplerInfo*  pSamplerInfo,
        void*               pOut);

    void Barrier(GfxCmdBuffer* pCmdBuf, CmdStream* pCmdStream, const BarrierInfo& barrier) const;
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
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void TransitionDepthStencil(
        GfxCmdBuffer*                 pCmdBuf,
        GfxCmdBufferState             cmdBufState,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void DescribeBarrier(
        GfxCmdBuffer*                 pCmdBuf,
        Developer::BarrierOperations* pOperations,
        const BarrierTransition*      pTransition) const;
   void DescribeBarrierStart(GfxCmdBuffer* pCmdBuf, uint32 reason) const;
   void DescribeBarrierEnd(GfxCmdBuffer* pCmdBuf, Developer::BarrierOperations* pOperations) const;

    const BoundGpuMemory& TrapHandler(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapHandler : m_computeTrapHandler; }
    const BoundGpuMemory& TrapBuffer(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapBuffer : m_computeTrapBuffer; }

#if DEBUG
    uint32* TemporarilyHangTheGpu(EngineType engineType, uint32 number, uint32* pCmdSpace) const override;
#endif

    int32 OverridedTileIndexForDepthStencilCopy(int32 tileIndex) const
        { return m_overridedTileIndexForDepthStencilCopy[tileIndex]; }

    static constexpr bool WaEnableDcc8bppWithMsaa = false;

    bool Support4VgtWithResetIdx()          const { return (m_supportFlags.support4VgtWithResetIdx == 1); }
    bool WaCpDmaHangMcTcAckDrop()           const { return (m_supportFlags.waCpDmaHangMcTcAckDrop == 1); }
    bool WaDbReZStencilCorruption()         const { return (m_supportFlags.waDbReZStencilCorruption == 1); }
    bool WaDbOverRasterization()            const { return (m_supportFlags.waDbOverRasterization == 1); }
    bool WaEventWriteEopPrematureL2Inv()    const { return (m_supportFlags.waEventWriteEopPrematureL2Inv == 1); }
    bool WaShaderSpiBarrierMgmt()           const { return (m_supportFlags.waShaderSpiBarrierMgmt == 1); }
    bool WaShaderSpiWriteShaderPgmRsrc2Ls() const { return (m_supportFlags.waShaderSpiWriteShaderPgmRsrc2Ls == 1); }
    bool WaMiscOffchipLdsBufferLimit()      const { return (m_supportFlags.waMiscOffchipLdsBufferLimit == 1); }
    bool WaMiscGsRingOverflow()             const { return (m_supportFlags.waMiscGsRingOverflow == 1); }
    bool WaMiscVgtNullPrim()                const { return (m_supportFlags.waMiscVgtNullPrim == 1); }
    bool WaMiscDccOverwriteComb()           const { return (m_supportFlags.waMiscDccOverwriteComb == 1); }
    bool WaEnableDccXthickUse()             const { return (m_supportFlags.waEnableDccXthickUse == 1); }
    bool WaDbDecompressOnPlanesFor4xMsaa()  const { return (m_supportFlags.waDbDecompressOnPlanesFor4xMsaa == 1); }
    bool WaAsyncComputeMoreThan4096ThreadGroups() const
        { return (m_supportFlags.waAsyncComputeMoreThan4096ThreadGroups == 1); }
    bool WaNoFastClearWithDcc()             const { return (m_supportFlags.waNoFastClearWithDcc == 1); }
    bool WaMiscMixedHeapFlips()             const { return (m_supportFlags.waMiscMixedHeapFlips == 1); }
    bool WaDbDecompressPerformance()        const { return (m_supportFlags.waDbDecompressPerformance == 1); }
    bool WaAlignCpDma()                     const { return (m_supportFlags.waAlignCpDma == 1); }
    bool WaForceToWriteNonRlcRestoredRegs() const { return (m_supportFlags.waForceToWriteNonRlcRestoredRegs == 1); }
    bool WaShaderOffChipGsHang()            const { return (m_supportFlags.waShaderOffChipGsHang == 1); }
    bool WaMiscVsBackPressure()             const { return (m_supportFlags.waMiscVsBackPressure == 1); }
    bool WaVgtPrimResetIndxMaskByType()     const { return (m_supportFlags.waVgtPrimResetIndxMaskByType == 1); }
    bool WaWaitIdleBeforeSpiConfigCntl()    const { return (m_supportFlags.waWaitIdleBeforeSpiConfigCntl == 1); }
    bool WaCpIb2ChainingUnsupported()       const { return (m_supportFlags.waCpIb2ChainingUnsupported == 1); }
    bool WaMiscNullIb()                     const { return (m_supportFlags.waMiscNullIb == 1); }
    bool WaCbNoLt16BitIntClamp()            const { return (m_supportFlags.waCbNoLt16BitIntClamp == 1); }

    Gfx8TcCompatDbFlushWorkaround WaDbTcCompatFlush() const { return m_waDbTcCompatFlush; }

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const override;

private:
    bool GetDepthStencilBltPerSubres(
        GfxCmdBuffer*            pCmdBuf,
        uint32*                  pBlt,
        const BarrierTransition& transition,
        bool                     earlyPhase) const;
    void DepthStencilExpand(
        GfxCmdBuffer*                 pCmdBuf,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        Developer::BarrierOperations* pOperations) const;
    void DepthStencilExpandHiZRange(
        GfxCmdBuffer*                 pCmdBuf,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void DepthStencilResummarize(
        GfxCmdBuffer*                 pCmdBuf,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        Developer::BarrierOperations* pOperations) const;

    uint32 GetColorBltPerSubres(
        GfxCmdBuffer*            pCmdBuf,
        uint32*                  pBlt,
        const BarrierTransition& transition,
        bool                     earlyPhase) const;
    void DccDecompress(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        Developer::BarrierOperations* pOperations) const;
    void FmaskDecompress(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        Developer::BarrierOperations* pOperations) const;
    void FastClearEliminate(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        Developer::BarrierOperations* pOperations) const;
    void MsaaDecompress(
        GfxCmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierTransition&      transition,
        const Image&                  gfx6Image,
        const SubresRange&            subresRange,
        uint32                        blt,
        Developer::BarrierOperations* pOperations) const;

    void FillCacheOperations(const SyncReqs& syncReqs, Developer::BarrierOperations* pBarrierOps) const;

    void SetupWorkarounds();

    Gfx6::CmdUtil     m_cmdUtil;
    Gfx6::RsrcProcMgr m_rsrcProcMgr;

    BoundGpuMemory    m_occlusionSrcMem;
    BoundGpuMemory    m_cpDmaPatchMem;

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

    // Overrided cb tile index for depth stencil copy. The tile index shall fits for cb while has compatible marco tile
    // mode with respect to db tile mode.
    int32 m_overridedTileIndexForDepthStencilCopy[8];

    // Store GPU memory and offsets for compute/graphics trap handlers and trap buffers.  Trap handlers are client-
    // installed hardware shaders that can be executed based on exceptions occurring in the main shader or in other
    // situations like supporting a debugger.  Trap buffers are just scratch memory that can be accessed from a trap
    // handler.  For simplicity, and to match future hardware behavior, only one trap handler can be bound at a time
    // for all shader stages for graphics/compute.
    BoundGpuMemory m_computeTrapHandler;
    BoundGpuMemory m_computeTrapBuffer;
    BoundGpuMemory m_graphicsTrapHandler;
    BoundGpuMemory m_graphicsTrapBuffer;

    struct
    {
        uint32 support4VgtWithResetIdx                : 1;
        uint32 waCpDmaHangMcTcAckDrop                 : 1;
        uint32 waDbReZStencilCorruption               : 1;
        uint32 waDbOverRasterization                  : 1;
        uint32 waEventWriteEopPrematureL2Inv          : 1;
        uint32 waShaderSpiBarrierMgmt                 : 1;
        uint32 waShaderSpiWriteShaderPgmRsrc2Ls       : 1;
        uint32 waMiscOffchipLdsBufferLimit            : 1;
        uint32 waMiscGsRingOverflow                   : 1;
        uint32 waMiscVgtNullPrim                      : 1;
        uint32 waMiscDccOverwriteComb                 : 1;
        uint32 waEnableDccXthickUse                   : 1;
        uint32 waDbDecompressOnPlanesFor4xMsaa        : 1;
        uint32 waAsyncComputeMoreThan4096ThreadGroups : 1;
        uint32 waNoFastClearWithDcc                   : 1;
        uint32 waMiscMixedHeapFlips                   : 1;
        uint32 waDbDecompressPerformance              : 1;
        uint32 waAlignCpDma                           : 1;
        uint32 waForceToWriteNonRlcRestoredRegs       : 1;
        uint32 waShaderOffChipGsHang                  : 1;
        uint32 waMiscVsBackPressure                   : 1;
        uint32 waVgtPrimResetIndxMaskByType           : 1;
        uint32 waWaitIdleBeforeSpiConfigCntl          : 1;
        uint32 waCpIb2ChainingUnsupported             : 1;
        uint32 waMiscNullIb                           : 1;
        uint32 waCbNoLt16BitIntClamp                  : 1;
        uint32 reserved                               : 6;
    } m_supportFlags;

    Gfx8TcCompatDbFlushWorkaround m_waDbTcCompatFlush;

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

static const Gfx6PalSettings& GetGfx6Settings(
    const Pal::Device& device)
{
    return static_cast<const Pal::Gfx6::Device*>(device.GetGfxDevice())->Settings();
}

} // Gfx6
} // Pal
