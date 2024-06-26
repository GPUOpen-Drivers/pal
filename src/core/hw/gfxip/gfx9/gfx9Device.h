/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Barrier.h"
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9CmdUtil.h"
#include "core/hw/gfxip/gfx9/gfx9MetaEq.h"
#include "core/hw/gfxip/gfx9/gfx9SettingsLoader.h"
#include "core/hw/gfxip/gfx9/gfx9ShaderRingSet.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "core/hw/gfxip/pm4CmdBuffer.h"
#include "core/hw/gfxip/rpm/gfx9/gfx9RsrcProcMgr.h"

#include "palPipelineAbi.h"
#include "palAutoBuffer.h"

#include <atomic>

namespace Pal
{

struct BarrierInfo;

namespace Gfx9
{

// Needed only for VRS support
class Gfx10DepthStencilView;

// This value is the result Log2(MaxMsaaRasterizerSamples) + 1.
constexpr uint32 MsaaLevelCount = 5;

enum LateAllocVsMode : uint32
{
    LateAllocVsDisable = 0x00000000,
    LateAllocVsInvalid = 0xFFFFFFFF,
};

enum RegisterRangeType : uint32
{
    RegRangeUserConfig           = 0x0,
    RegRangeContext              = 0x1,
    RegRangeSh                   = 0x2,
    RegRangeCsSh                 = 0x3,
    RegRangeNonShadowed          = 0x4,
    RegRangeCpRs64InitSh         = 0x5,
    RegRangeCpRs64InitCsSh       = 0x6,
    RegRangeCpRs64InitUserConfig = 0x7,
};

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
    const uint32         regAddr,
    const uint16         baseUserDataReg,
    const uint32         value,
    PackedRegisterPair*  pValidRegPairs,
    UserDataEntryLookup* pRegLookup,
    uint32               minLookupValue,
    uint32*              pNumValidRegs)
{
    SetOnePackedRegPairLookup<PERSISTENT_SPACE_START>(regAddr,
                                                      baseUserDataReg,
                                                      value,
                                                      pValidRegPairs,
                                                      pRegLookup,
                                                      minLookupValue,
                                                      pNumValidRegs);
}

// =====================================================================================================================
// Sets offsets and values of a sequence of consecutive user data entries in packed register pairs.
static void SetSeqUserDataEntryPairPackedValues(
    const uint32         startAddr,
    const uint32         endAddr,
    const uint16         baseUserDataReg,
    const void*          pValues,
    PackedRegisterPair*  pValidRegPairs,
    UserDataEntryLookup* pRegLookup,
    uint32               minLookupValue,
    uint32*              pNumValidRegs)
{
    SetSeqPackedRegPairLookup<PERSISTENT_SPACE_START>(startAddr,
                                                      endAddr,
                                                      baseUserDataReg,
                                                      pValues,
                                                      pValidRegPairs,
                                                      pRegLookup,
                                                      minLookupValue,
                                                      pNumValidRegs);
}

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

    virtual Result InitSettings() const override
    {
        return static_cast<Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->Init();
    }

    virtual Util::MetroHash::Hash GetSettingsHash() const override
    {
        Util::MetroHash::Hash zeroHash = {};
        return (m_pDdSettingsLoader != nullptr) ?
            static_cast<Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->GetSettingsHash() : zeroHash;
    }

    //            rbAligned must be true for ASICs with > 1 RBs, otherwise there would be access violation
    //            between different RBs
    virtual bool IsRbAligned() const
        { return ((GetNumRbsPerSeLog2() + GetNumShaderEnginesLog2()) != 0); }

    virtual void HwlValidateSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->ValidateSettings(pSettings);
    }

    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->OverrideDefaults(pSettings);
    }

    virtual void HwlRereadSettings() override {}

    virtual void HwlReadSettings() override
    {
        static_cast<Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->ReadSettings();
    }

    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const override;

    virtual Result GetLinearImageAlignments(LinearImageAlignments* pAlignments) const override;

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
        return static_cast<const Pal::Gfx9::SettingsLoader*>(m_pDdSettingsLoader)->GetSettings();
    }

    static uint32 CalcNumRecords(
        size_t      sizeInBytes,
        uint32      stride);

    // Gets the memory object used to accelerate occlusion query resets.
    const BoundGpuMemory& OcclusionResetMem() const { return m_occlusionSrcMem; }

    // Gets the memory object for vertex attributes
    const BoundGpuMemory& VertexAttributesMem(bool isTmz) const { return m_vertexAttributesMem[isTmz]; }

    static uint16 GetBaseUserDataReg(HwShaderStage shaderStage);

    // Gets a copy of the reset value for a single occlusion query slot. The caller is responsible for determining the
    // size of the slot so that they do not read past the end of this buffer.
    const uint32* OcclusionSlotResetValue() const
        { return reinterpret_cast<const uint32*>(m_occlusionSlotResetValues); }

    virtual Result SetSamplePatternPalette(const SamplePatternPalette& palette) override;
    void GetSamplePatternPalette(SamplePatternPalette* pSamplePatternPalette);

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const override;

    virtual DccFormatEncoding ComputeDccFormatEncoding(
        const SwizzledFormat& swizzledFormat,
        const SwizzledFormat* pViewFormats,
        uint32                viewFormatCount) const override;

    uint32 GetCuEnableMaskHi(uint32 disabledCuMmask, uint32 enabledCuMaskSetting) const;

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
    static void PAL_STDCALL Gfx10DecodeBufferViewSrd(
        const IDevice*  pDevice,
        const void*     pBufferViewSrd,
        BufferViewInfo* pViewInfo);

    static void PAL_STDCALL Gfx10DecodeImageViewSrd(
        const IDevice*   pDevice,
        const IImage*    pImage,
        const void*      pImageViewSrd,
        DecodedImageSrd* pDecodedInfo);

    const regGB_ADDR_CONFIG& GetGbAddrConfig() const;

    uint32 GetMaxFragsLog2() const         { return GetGbAddrConfig().bits.MAX_COMPRESSED_FRAGS; }
    uint32 GetNumPipesLog2() const         { return GetGbAddrConfig().bits.NUM_PIPES; }
    uint32 GetNumShaderEnginesLog2() const { return GetGbAddrConfig().bits.NUM_SHADER_ENGINES; }
    uint32 GetNumRbsPerSeLog2() const      { return GetGbAddrConfig().bits.NUM_RB_PER_SE; }
    uint32 GetNumPkrsLog2() const          { return GetGbAddrConfig().gfx103PlusExclusive.NUM_PKRS; }

    uint32 GetPipeInterleaveLog2() const;

    // This is the granularity that LDS is actually allocated in in terms of bytes
    uint32 GetHwAllocLdsGranularityBytes() const { return Parent()->ChipProperties().gfxip.ldsGranularity; }

    static uint32 GetMaxWavesPerSh(const GpuChipProperties& chipProps, bool isCompute);

    static uint32 GetBinSizeEnum(uint32  binSize);

    uint32 ComputeNoTessPrimGroupSize(uint32 targetPrimGroupSize) const;
    uint32 ComputeNoTessPatchPrimGroupSize(uint32 patchControlPoints) const;
    uint32 ComputeTessPrimGroupSize(uint32 numPatchesPerThreadGroup) const;

    void IncreaseMsaaHistogram(uint32 samples) override;
    void DecreaseMsaaHistogram(uint32 samples) override;
    bool UpdateSppState(const IImage& presentableImage) override;
    uint32 GetPixelCount() const override { return m_presentResolution.height * m_presentResolution.width; }
    uint32 GetMsaaRate() const override { return m_msaaRate; }

    bool UseStateShadowing(EngineType engineType) const;

    bool   UseFixedLateAllocVsLimit() const { return m_useFixedLateAllocVsLimit; }
    uint32 LateAllocVsLimit() const { return m_lateAllocVsLimit; }

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

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const override;

    const Gfx10DepthStencilView* GetVrsDepthStencilView();

    uint32 Gfx103PlusExclusiveGetNumActiveShaderArraysLog2() const;

    virtual uint32 GetVarBlockSize() const override { return m_varBlockSize; }

    uint32 GetShaderPrefetchSize(size_t shaderSizeBytes) const;

    uint32 BufferSrdResourceLevel() const;

    Result AllocateVertexAttributesMem(bool isTmz);

    virtual ClearMethod GetDefaultSlowClearMethod(
        const ImageCreateInfo&  createInfo,
        const SwizzledFormat& clearFormat) const override;

    const BarrierMgr* BarrierMgr() const { return &m_barrierMgr; }

    virtual bool DisableAc01ClearCodes() const override;

    virtual void UpdateDisplayDcc(
        GfxCmdBuffer*                   pCmdBuf,
        const CmdPostProcessFrameInfo&  postProcessInfo,
        bool*                           pAddedGpuWork) const override;

    bool EnableReleaseMemWaitCpDma() const { return Settings().gfx11EnableReleaseMemWaitCpDma; }

    const GraphicsPipelineSignature& GetNullGfxSignature() const { return m_nullGfxSignature; }
    const ComputeShaderSignature& GetNullCsSignature() const { return m_nullCsSignature; }

private:
    void Gfx10SetImageSrdDims(sq_img_rsrc_t*  pSrd, uint32 width, uint32  height) const;

    void SetSrdBorderColorPtr(
        sq_img_samp_t*  pSrd,
        uint32          borderColorPtr) const;

    Result InitOcclusionResetMem();

    void SetupWorkarounds();

    Gfx9::CmdUtil    m_cmdUtil;
    Gfx9::BarrierMgr m_barrierMgr;

    BoundGpuMemory m_occlusionSrcMem;   // If occlusionQueryDmaBufferSlots is in use, this is the source memory.

    // Tracks the sample pattern palette for sample pos shader ring. Access to this object must be
    // serialized using m_samplePatternLock.
    volatile SamplePatternPalette m_samplePatternPalette;

    // An image of reset values for an entire occlusion query slot
    OcclusionQueryResultPair m_occlusionSlotResetValues[MaxNumRbs];

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

    mutable std::atomic<uint32> m_nextColorTargetViewId;
    mutable std::atomic<uint32> m_nextDepthStencilViewId;

    // 0 - Non-TMZ, 1 - TMZ
    BoundGpuMemory m_vertexAttributesMem[2];

    bool   m_useFixedLateAllocVsLimit;
    uint32 m_lateAllocVsLimit;

    GraphicsPipelineSignature   m_nullGfxSignature;
    ComputeShaderSignature      m_nullCsSignature;

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
