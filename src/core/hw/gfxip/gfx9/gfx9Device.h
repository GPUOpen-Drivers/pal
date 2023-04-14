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

#pragma once

#include "core/device.h"
#include "g_gfx9Settings.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9MetaEq.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.h"

#include "palPipelineAbi.h"
#include "palAutoBuffer.h"

#include <atomic>

namespace Pal
{

class  Pm4CmdBuffer;
struct BarrierInfo;

namespace Gfx9
{

// Needed only for VRS support
class Gfx10DepthStencilView;

enum class AcquirePoint : uint8;

// This value is the result Log2(MaxMsaaRasterizerSamples) + 1.
constexpr uint32 MsaaLevelCount = 5;

enum RegisterRangeType : uint32
{
    RegRangeUserConfig           = 0x0,
    RegRangeContext              = 0x1,
    RegRangeSh                   = 0x2,
    RegRangeCsSh                 = 0x3,
    RegRangeNonShadowed          = 0x4,
#if PAL_BUILD_GFX11
    RegRangeCpRs64InitSh         = 0x5,
    RegRangeCpRs64InitCsSh       = 0x6,
    RegRangeCpRs64InitUserConfig = 0x7,
#endif
};

struct SyncReqs
{
    // These flags describe which caches must be flushed and/or invalidated.
    SyncGlxFlags glxCaches;
    SyncRbFlags  rbCaches;

    // These flags describe which syncronization operations are required.
    struct
    {
        uint8 waitOnEopTs    : 1;
        uint8 cbTargetStall  : 1; // Gfx-only: Do a range-checked stall on the active color targets.
        uint8 dbTargetStall  : 1; // Gfx-only: Do a range-checked stall on the active depth and stencil targets.
        uint8 vsPartialFlush : 1; // Gfx-only
        uint8 psPartialFlush : 1; // Gfx-only
        uint8 csPartialFlush : 1;
        uint8 pfpSyncMe      : 1; // Gfx-only
        uint8 syncCpDma      : 1;
    };
};

enum HwLayoutTransition : uint8
{
    None                         = 0x0,

    // Depth/Stencil
    ExpandDepthStencil           = 0x1,
    HwlExpandHtileHiZRange       = 0x2,
    ResummarizeDepthStencil      = 0x3,

    // Color
    FastClearEliminate           = 0x4,
    FmaskDecompress              = 0x5,
    DccDecompress                = 0x6,
    MsaaColorDecompress          = 0x7,

    // Initialize Color metadata or Depth/stencil Htile.
    InitMaskRam                  = 0x8,
};

// Information for layout transition BLT
struct LayoutTransitionInfo
{
    union
    {
        struct
        {
            uint32 useComputePath   : 1;  // For those transition BLTs that could do either graphics or compute path,
                                          // figure out what path the BLT will use and cache it here.
            uint32 reserved         : 7;  // Reserved for future usage.
        };
        uint8 u8All;                      // Flags packed as uint32.
    } flags;

    HwLayoutTransition blt[2];            // Color target may need a second decompress pass to do MSAA color decompress.
};

// A structure that helps cache and reuse the calculated BLT transition and sync requests for an image barrier in
// acquire-release based barrier.
struct AcqRelImgTransitionInfo
{
    ImgBarrier           imgBarrier;
    LayoutTransitionInfo layoutTransInfo;
    uint32               stageMask;     // Pipeline stage mask of layoutTransInfo.blt[0]
    uint32               accessMask;    // Coherency access mask of layoutTransInfo.blt[0]
};

using AcqRelAutoBuffer = Util::AutoBuffer<AcqRelImgTransitionInfo, 8, Platform>;

// Acquire release transition info gathered from all image transitions.
struct AcqRelTransitionInfo
{
    AcqRelAutoBuffer* pBltList;       // List of AcqRelImgTransitionInfo that need layout transition BLT.
    uint32            bltCount;       // Number of valid entries in pBltList.
    uint32            bltStageMask;   // Pipeline stage mask for all layout transition BLTs in pBltList.
    uint32            bltAccessMask;  // Coherency access mask for all layout transition BLTs in pBltList.
    bool              hasMetadata;    // If any image of the transition images has metadata.
};

// Forward decl
static const Gfx9PalSettings& GetGfx9Settings(const Pal::Device& device);

#if PAL_BUILD_GFX11
// =====================================================================================================================
// Sets an offset and value in a packed context register pair.
static void SetOneContextRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              regAddr,
    uint32              value)
{
    SetOneRegValPairPacked<CONTEXT_SPACE_START>(pRegPairs, pIndex, regAddr, value);
}

// =====================================================================================================================
// Sets offsets and values for a sequence of consecutive context registers in packed register pairs.
static void SetSeqContextRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              startAddr,
    uint16              endAddr,
    const void*         pValues)
{
    SetSeqRegValPairPacked<CONTEXT_SPACE_START>(pRegPairs, pIndex, startAddr, endAddr, pValues);
}

// =====================================================================================================================
// Sets an offset and value in a packed SH register pair.
static void SetOneShRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              regAddr,
    uint32              value)
{
    SetOneRegValPairPacked<PERSISTENT_SPACE_START>(pRegPairs, pIndex, regAddr, value);
}

// =====================================================================================================================
// Sets offsets and values for a sequence of consecutive SH registers in packed register pairs.
static void SetSeqShRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              startAddr,
    uint16              endAddr,
    const void*         pValues)
{
    SetSeqRegValPairPacked<PERSISTENT_SPACE_START>(pRegPairs, pIndex, startAddr, endAddr, pValues);
}

// =====================================================================================================================
// Sets the offset and value of a user data entry in a packed register pair.
static void SetOneUserDataEntryPairPackedValue(
    const uint32        regAddr,
    const uint16        baseUserDataReg,
    const uint32        value,
    PackedRegisterPair* pValidRegPairs,
    uint8*              pValidRegsLookup,
    uint32*             pNumValidRegs)
{
    SetOnePackedRegPairLookup<PERSISTENT_SPACE_START>(regAddr,
                                                      baseUserDataReg,
                                                      value,
                                                      pValidRegPairs,
                                                      pValidRegsLookup,
                                                      pNumValidRegs);
}

// =====================================================================================================================
// Sets offsets and values of a sequence of consecutive user data entries in packed register pairs.
static void SetSeqUserDataEntryPairPackedValues(
    const uint32        startAddr,
    const uint32        endAddr,
    const uint16        baseUserDataReg,
    const void*         pValues,
    PackedRegisterPair* pValidRegPairs,
    uint8*              pValidRegsLookup,
    uint32*             pNumValidRegs)
{
    SetSeqPackedRegPairLookup<PERSISTENT_SPACE_START>(startAddr,
                                                      endAddr,
                                                      baseUserDataReg,
                                                      pValues,
                                                      pValidRegPairs,
                                                      pValidRegsLookup,
                                                      pNumValidRegs);
}
#endif

// =====================================================================================================================
// GFX9 hardware layer implementation of GfxDevice. Responsible for creating HW-specific objects such as Queue contexts
// and owning child objects such as the SC manager.
class Device final : public GfxDevice
{
public:
    explicit Device(Pal::Device* pDevice);
    virtual ~Device() { }

    virtual Result EarlyInit() override;
    virtual Result LateInit() override;
    virtual Result Finalize() override;
    virtual Result Cleanup() override;

    virtual Result HwlValidateImageViewInfo(const ImageViewInfo& viewInfo) const override;
    virtual Result HwlValidateSamplerInfo(const SamplerInfo& samplerInfo)  const override;

    //            rbAligned must be true for ASICs with > 1 RBs, otherwise there would be access violation
    //            between different RBs
    virtual bool IsRbAligned() const
        { return ((GetNumRbsPerSeLog2() + GetNumShaderEnginesLog2()) != 0); }

    virtual void HwlValidateSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pSettingsLoader)->ValidateSettings(pSettings);
    }

    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pSettingsLoader)->OverrideDefaults(pSettings);
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
        const QueueCreateInfo& createInfo,
        Engine*                pEngine,
        void*                  pPlacementAddr,
        QueueContext**         ppQueueContext) override;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const override;
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        bool                             isInternal,
        IPipeline**                      ppPipeline) override;

   virtual size_t GetShaderLibrarySize(
        const ShaderLibraryCreateInfo&  createInfo,
        Result*                         pResult) const override;
    virtual Result CreateShaderLibrary(
        const ShaderLibraryCreateInfo&  createInfo,
        void*                           pPlacementAddr,
        bool                            isInternal,
        IShaderLibrary**                ppPipeline) override;

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
        const ColorTargetViewCreateInfo&   createInfo,
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

    virtual bool SupportsIterate256() const override;

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

#if PAL_BUILD_GFX11
    // Gets the memory object for vertex attributes
    const BoundGpuMemory& VertexAttributesMem(bool isTmz) const { return m_vertexAttributesMem[isTmz]; }
#endif

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

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const override;

    virtual bool IsImageFormatOverrideNeeded(
        const ImageCreateInfo& imageCreateInfo,
        ChNumFormat*           pFormat,
        uint32*                pPixelsPerBlock) const override;

    virtual DccFormatEncoding ComputeDccFormatEncoding(
        const SwizzledFormat& swizzledFormat,
        const SwizzledFormat* pViewFormats,
        uint32                viewFormatCount) const override;

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

    uint32 GetCuEnableMaskHi(uint32 disabledCuMmask, uint32 enabledCuMaskSetting) const;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 789
    static uint16 AdjustCuEnHi(uint16  val, uint32  mask) { return ((val & mask) >> 16); }
#endif

    // Function definition for creating typed buffer view SRDs.
    static void PAL_STDCALL Gfx10CreateTypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating untyped buffer view SRDs.
    static void PAL_STDCALL Gfx10CreateUntypedBufferViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const BufferViewInfo* pBufferViewInfo,
        void*                 pOut);

    // Function definition for creating image view SRDs.
    static void PAL_STDCALL Gfx10CreateImageViewSrds(
        const IDevice*       pDevice,
        uint32               count,
        const ImageViewInfo* pImgViewInfo,
        void*                pOut);

    // Function definition for creating a sampler SRD.
    static void PAL_STDCALL Gfx10CreateSamplerSrds(
        const IDevice*      pDevice,
        uint32              count,
        const SamplerInfo*  pSamplerInfo,
        void*               pOut);

    void Gfx10CreateFmaskViewSrdsInternal(
        const FmaskViewInfo&          viewInfo,
        const FmaskViewInternalInfo*  pFmaskViewInternalInfo,
        sq_img_rsrc_t*                pSrd) const;

    // Function definition for creating fmask view SRDs.
    static void PAL_STDCALL CreateFmaskViewSrds(
        const IDevice*        pDevice,
        uint32                count,
        const FmaskViewInfo*  pFmaskViewInfo,
        void*                 pOut);

    static void PAL_STDCALL CreateBvhSrds(
        const IDevice*  pDevice,
        uint32          count,
        const BvhInfo*  pBvhInfo,
        void*           pOut);

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

    void Barrier(Pm4CmdBuffer* pCmdBuf, CmdStream* pCmdStream, const BarrierInfo& barrier) const;

    AcqRelSyncToken Release(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     releaseInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    void Acquire(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        syncTokenCount,
        const AcqRelSyncToken*        pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const;

    void ReleaseEvent(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     releaseInfo,
        const IGpuEvent*              pClientEvent,
        Developer::BarrierOperations* pBarrierOps) const;

    void AcquireEvent(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     acquireInfo,
        uint32                        gpuEventCount,
        const IGpuEvent* const*       ppGpuEvents,
        Developer::BarrierOperations* pBarrierOps) const;

    void ReleaseThenAcquire(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     barrierInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    void FillCacheOperations(const SyncReqs& syncReqs, Developer::BarrierOperations* pOperations) const;

    void IssueSyncs(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        SyncReqs                      syncReqs,
        HwPipePoint                   waitPoint,
        gpusize                       rangeBase,
        gpusize                       rangeSize,
        Developer::BarrierOperations* pOperations) const;
    void FlushAndInvL2IfNeeded(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        Developer::BarrierOperations* pOperations) const;
    void ExpandColor(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void TransitionDepthStencil(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        Pm4CmdBufferStateFlags        cmdBufStateFlags,
        const BarrierInfo&            barrier,
        uint32                        transitionId,
        bool                          earlyPhase,
        SyncReqs*                     pSyncReqs,
        Developer::BarrierOperations* pOperations) const;
    void AcqRelColorTransition(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const ImgBarrier&             imgBarrier,
        LayoutTransitionInfo          layoutTransInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    void AcqRelColorTransitionEvent(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const ImgBarrier&             imgBarrier,
        LayoutTransitionInfo          layoutTransInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    void AcqRelDepthStencilTransition(
        Pm4CmdBuffer*                 pCmdBuf,
        const ImgBarrier&             imgBarrier,
        LayoutTransitionInfo          layoutTransInfo) const;
    void DescribeBarrier(
        Pm4CmdBuffer*                 pCmdBuf,
        const BarrierTransition*      pTransition,
        Developer::BarrierOperations* pOperations) const;
    void DescribeBarrierStart(
        Pm4CmdBuffer*                 pCmdBuf,
        uint32                        reason,
        Developer::BarrierType        type) const;
    void DescribeBarrierEnd(Pm4CmdBuffer* pCmdBuf, Developer::BarrierOperations* pOperations) const;

    const regGB_ADDR_CONFIG& GetGbAddrConfig() const;

    uint32 GetMaxFragsLog2() const         { return GetGbAddrConfig().bits.MAX_COMPRESSED_FRAGS; }
    uint32 GetNumPipesLog2() const         { return GetGbAddrConfig().bits.NUM_PIPES; }
    uint32 GetNumShaderEnginesLog2() const { return GetGbAddrConfig().bits.NUM_SHADER_ENGINES; }
    uint32 GetNumRbsPerSeLog2() const      { return GetGbAddrConfig().bits.NUM_RB_PER_SE; }

    uint32 GetPipeInterleaveLog2() const;

    uint32 GetDbDfsmControl() const;

    // This is the granularity that LDS is actually allocated in in terms of bytes
    uint32 GetHwAllocLdsGranularityBytes() const { return Parent()->ChipProperties().gfxip.ldsGranularity; }

    static uint32 GetMaxWavesPerSh(const GpuChipProperties& chipProps, bool isCompute);

    const BoundGpuMemory& TrapHandler(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapHandler : m_computeTrapHandler; }
    const BoundGpuMemory& TrapBuffer(PipelineBindPoint pipelineType) const override
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapBuffer : m_computeTrapBuffer; }

    static uint32 GetBinSizeEnum(uint32  binSize);

    uint32 ComputeNoTessPrimGroupSize(uint32 targetPrimGroupSize) const;
    uint32 ComputeNoTessPatchPrimGroupSize(uint32 patchControlPoints) const;
    uint32 ComputeTessPrimGroupSize(uint32 numPatchesPerThreadGroup) const;

    void IncreaseMsaaHistogram(uint32 samples) override;
    void DecreaseMsaaHistogram(uint32 samples) override;
    bool UpdateSppState(const IImage& presentableImage) override;
    uint32 GetPixelCount() const override { return m_presentResolution.height * m_presentResolution.width; }
    uint32 GetMsaaRate() const override { return m_msaaRate; }

    bool NeedGlobalFlushAndInvL2(
        uint32        srcCacheMask,
        uint32        dstCacheMask,
        const IImage* pImage) const;

    bool UseStateShadowing(EngineType engineType) const;

#if DEBUG
    uint32* TemporarilyHangTheGpu(EngineType engineType, uint32 number, uint32* pCmdSpace) const override;
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

    PM4_PFP_CONTEXT_CONTROL GetContextControl() const;

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

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const override;

    const Gfx10DepthStencilView* GetVrsDepthStencilView();

    uint32 Gfx103PlusExclusiveGetNumActiveShaderArraysLog2() const;

    virtual uint32 GetVarBlockSize() const override { return m_varBlockSize; }

#if  PAL_BUILD_GFX11
    uint32 GetShaderPrefetchSize(size_t  shaderSizeBytes) const;
#endif

    uint32 BufferSrdResourceLevel() const;

#if PAL_BUILD_GFX11
    Result AllocateVertexAttributesMem(bool isTmz);
#endif

private:
    void Gfx10SetImageSrdDims(sq_img_rsrc_t*  pSrd, uint32 width, uint32  height) const;

    void SetSrdBorderColorPtr(
        sq_img_samp_t*  pSrd,
        uint32          borderColorPtr) const;

    Result InitOcclusionResetMem();

    void Gfx9CreateFmaskViewSrdsInternal(
        const FmaskViewInfo&         viewInfo,
        const FmaskViewInternalInfo* pFmaskViewInternalInfo,
        Gfx9ImageSrd*                pSrd) const;

    void SetupWorkarounds();

    LayoutTransitionInfo PrepareColorBlt(
        Pm4CmdBuffer*       pCmdBuf,
        const Pal::Image&   image,
        const SubresRange&  subresRange,
        ImageLayout         oldLayout,
        ImageLayout         newLayout) const;
    LayoutTransitionInfo PrepareDepthStencilBlt(
        const Pm4CmdBuffer* pCmdBuf,
        const Pal::Image&   image,
        const SubresRange&  subresRange,
        ImageLayout         oldLayout,
        ImageLayout         newLayout) const;
    LayoutTransitionInfo PrepareBltInfo(
        Pm4CmdBuffer*       pCmdBuf,
        const ImgBarrier&   imageBarrier) const;

    SyncGlxFlags GetAcquireCacheFlags(
        uint32                        accessMask,
        bool                          refreshTcc,
        bool                          mayHaveMetadata,
        Developer::BarrierOperations* pBarrierOps) const;

    SyncGlxFlags GetReleaseThenAcquireCacheFlags(
        uint32                        srcAccessMask,
        uint32                        dstAccessMask,
        bool                          refreshTcc,
        bool                          mayHaveMetadata,
        Developer::BarrierOperations* pBarrierOps) const;

    bool IssueBlt(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const ImgBarrier*             pImgBarrier,
        LayoutTransitionInfo          transition,
        bool*                         pPreInitHtileSynced,
        Developer::BarrierOperations* pBarrierOps) const;

    bool GetAcqRelLayoutTransitionBltInfo(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        const AcquireReleaseInfo&     barrierInfo,
        AcqRelTransitionInfo*         pTransitionInfo,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
        uint32*                       pSrcStageMask,
        uint32*                       pDstStageMask,
#endif
        uint32*                       pSrcAccessMask,
        uint32*                       pDstAccessMask,
        Developer::BarrierOperations* pBarrierOps) const;

    bool IssueAcqRelLayoutTransitionBlt(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        AcqRelTransitionInfo*         pTransitonInfo,
        Developer::BarrierOperations* pBarrierOps) const;

    bool AcqRelInitMaskRam(
        Pm4CmdBuffer*      pCmdBuf,
        CmdStream*         pCmdStream,
        const ImgBarrier&  imgBarrier) const;

    AcqRelSyncToken IssueReleaseSync(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        stageMask,
        uint32                        accessMask,
        bool                          refreshTcc,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueAcquireSync(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        stageMask,
        uint32                        accessMask,
        bool                          refreshTcc,
        bool                          mayHaveMetadata,
        uint32                        syncTokenCount,
        const AcqRelSyncToken*        pSyncTokens,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueReleaseThenAcquireSync(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        srcStageMask,
        uint32                        dstStageMask,
        uint32                        srcAccessMask,
        uint32                        dstAccessMask,
        bool                          refreshTcc,
        bool                          mayHaveMetadata,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueReleaseSyncEvent(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        stageMask,
        uint32                        accessMask,
        bool                          refreshTcc,
        const IGpuEvent*              pGpuEvent,
        Developer::BarrierOperations* pBarrierOps) const;

    void IssueAcquireSyncEvent(
        Pm4CmdBuffer*                 pCmdBuf,
        CmdStream*                    pCmdStream,
        uint32                        stageMask,
        uint32                        accessMask,
        bool                          refreshTcc,
        bool                          mayHaveMetadata,
        uint32                        gpuEventCount,
        const IGpuEvent* const*       ppGpuEvents,
        Developer::BarrierOperations* pBarrierOps) const;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 767
    void OptimizeReadOnlyMemBarrier(Pm4CmdBuffer* pCmdBuf, MemBarrier* pTransition) const;
    bool OptimizeReadOnlyImgBarrier(Pm4CmdBuffer* pCmdBuf, ImgBarrier* pTransition) const;
#endif

    AcquirePoint GetAcquirePoint(uint32 dstStageMask, EngineType engineType) const;

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

    // PAL will track the current application states: Resolution and MSAA rate.
    // MSAA will be determined by tracking images created that support being bound as a color target.
    // A histogram of 1X, 2X, 4X, 8X, 16X samples will be tracked, incrementing the appropriate value on image creation
    // and decrementing the appropriate value on destruction.
    volatile uint32         m_msaaHistogram[MsaaLevelCount];
    // MSAA rate will be the MSAA level of the "pile" that is the highest in the histogram.
    volatile uint32         m_msaaRate;
    // Resolution will be determined at each present by examining the width and height of the presented image.
    volatile Extent2d       m_presentResolution;

    Result  CreateVrsDepthView();
    void    DestroyVrsDepthImage(Pal::Image*  pDsImage);

    Gfx10DepthStencilView*  m_pVrsDepthView;
    bool                    m_vrsDepthViewMayBeNeeded;

    // Local copy of the GB_ADDR_CONFIG register
    const uint32      m_gbAddrConfig;
    const GfxIpLevel  m_gfxIpLevel;
    uint32            m_varBlockSize;

    uint16         m_firstUserDataReg[HwShaderStage::Last];

    mutable std::atomic<uint32> m_nextColorTargetViewId;
    mutable std::atomic<uint32> m_nextDepthStencilViewId;

#if PAL_BUILD_GFX11
    // 0 - Non-TMZ, 1 - TMZ
    BoundGpuMemory m_vertexAttributesMem[2];
#endif

    PAL_DISALLOW_DEFAULT_CTOR(Device);
    PAL_DISALLOW_COPY_AND_ASSIGN(Device);
};

static const Gfx9PalSettings& GetGfx9Settings(const Pal::Device& device)
{
    return static_cast<const Pal::Gfx9::Device*>(device.GetGfxDevice())->Settings();
}

extern bool IsBufferBigPageCompatible(
    const GpuMemory& gpuMemory,
    gpusize          offset,
    gpusize          extent,
    uint32           bigPageUsageMask);
extern bool IsImageBigPageCompatible(
    const Image& image,
    uint32       bigPageUsageMask);
extern bool IsFmaskBigPageCompatible(
    const Image& image,
    uint32       bigPageUsageMask);

} // Gfx9
} // Pal
