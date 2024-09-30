/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palDevice.h"
#include "palMetroHash.h"
#include "palSettingsLoader.h"
#include "core/cmdStream.h"
#include "core/platform.h"
#include "palHashMap.h"
#include "palSysMemory.h"
#include "core/hw/gfxip/pipelineLoader.h"

typedef union _ADDR_CREATE_FLAGS ADDR_CREATE_FLAGS;
typedef struct _ADDR_REGISTER_VALUE ADDR_REGISTER_VALUE;
typedef struct _ADDR_CREATE_INPUT ADDR_CREATE_INPUT;

namespace Util { enum SystemAllocType : uint32; }
namespace Util { class File; }
namespace Util { class FileView; }
namespace Util { class Mutex; }
namespace DevDriver { class SettingsBase; }

namespace Pal
{
class      CmdBuffer;
class      CmdStream;
class      CmdUploadRing;
class      ColorBlendState;
class      CompoundState;
class      ComputePipeline;
class      DepthStencilState;
class      Device;
class      Engine;
class      GfxImage;
class      GfxCmdBuffer;
class      GraphicsPipeline;
class      IBorderColorPalette;
class      IColorBlendState;
class      IColorTargetView;
class      IDepthStencilState;
class      IDepthStencilView;
class      Image;
class      IMsaaState;
class      IPerfExperiment;
class      IPipeline;
class      IQueryPool;
class      IShader;
class      MsaaState;
class      Platform;
class      Queue;
class      QueueContext;
class      RasterState;
class      RsrcProcMgr;
class      ScissorState;
class      ViewportState;

struct     BorderColorPaletteCreateInfo;
struct     BufferViewInfo;
struct     CmdBufferCreateInfo;
struct     CmdUploadRingCreateInfo;
struct     ColorBlendStateCreateInfo;
struct     ColorTargetViewCreateInfo;
struct     ComputePipelineCreateInfo;
struct     DepthStencilStateCreateInfo;
struct     DepthStencilViewCreateInfo;
struct     FmaskViewInfo;
struct     GpuChipProperties;
struct     GraphicsPipelineCreateInfo;
struct     ImageCreateInfo;
struct     ImageInfo;
struct     ImageViewInfo;
struct     MsaaStateCreateInfo;
struct     PerfExperimentCreateInfo;
struct     QueryPoolCreateInfo;
struct     PalSettings;
struct     RasterStateCreateInfo;
struct     SamplerInfo;
struct     ScShaderMem;
struct     ScissorStateCreateInfo;
struct     ShaderCreateInfo;
struct     ViewportStateCreateInfo;

enum class PipelineBindPoint : uint32;
enum class ShaderType : uint32;
enum class DccFormatEncoding : uint32;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 853
enum class Blend : uint8;
#else
enum class Blend : uint32;
#endif
enum class ClearMethod : uint32;

// Additional information for creating PAL-internal color target views.
struct ColorTargetViewInternalCreateInfo
{
    union
    {
        struct
        {
            uint32 dccDecompress            : 1;  // Indicates this color target view is for a DCC decompress
            uint32 fastClearElim            : 1;  // Indicates this color target view is for a fast-clear-eliminate
            uint32 fmaskDecompress          : 1;  // Indicates this color target view is for a fmask decompress
            uint32 depthStencilCopy         : 1;  // Indicates this color target view is for a depth/stencil copy
            uint32 disableClientCompression : 1;  // True to disable client compression
            uint32 reserved                 : 27; // Reserved for future use
        };
        uint32 u32All;
    } flags;
};

// Additional information for creating PAL-internal depth stencil views.
struct DepthStencilViewInternalCreateInfo
{
    float depthClearValue;   // value the depth buffer is cleared to, only valid if isDepthClear
    uint8 stencilClearValue; // value the stencil buffer is cleared to, only valid if isStencilClear

    union
    {
        struct
        {
            uint32 isExpand                 : 1;  // true if setting up an expand operation
            uint32 isDepthClear             : 1;  // true if this is a fast-depth clear
            uint32 isStencilClear           : 1;  // true if this is a fast-stencil clear
            uint32 isDepthCopy              : 1;  // true if this is a depth copy
            uint32 isStencilCopy            : 1;  // true if this is a stencil copy
            uint32 disableClientCompression : 1;  // true to disable client compression
            uint32 reserved                 : 26; // reserved, set to zero
        };
        uint32 u32All;
    } flags;
};

// Additional information for creating PAL-internal Fmask views.
struct FmaskViewInternalInfo
{
    union
    {
        struct
        {
            uint32 fmaskAsUav : 1;  // Setup FMask as a raw UAV. Used by RPM blits.
            uint32 reserved   : 31; // Reserved for future use.
        };
        uint32 u32All;
    } flags;
};

// Additional information for creating PAL-internal graphics pipelines.
struct GraphicsPipelineInternalCreateInfo
{
    union
    {
        struct
        {
            uint32 fastClearElim     :  1; // Fast clear eliminate BLT.
            uint32 fmaskDecompress   :  1; // FMask decompress BLT.
            uint32 dccDecompress     :  1; // DCC decompress BLT.
            uint32 resolveFixedFunc  :  1; // Fixed function resolve.
            uint32 isPartialPipeline :  1; // True if it is a partial pipeline in Graphics shader library
            uint32 reserved          : 27;
        };
        uint32 u32All;
    } flags;
};

// Additional information for creating PAL-internal compound state.
struct CompoundStateInternalCreateInfo
{
    GraphicsPipelineInternalCreateInfo gfxPipelineInfo;
};

// Defines a range of registers to be loaded from state-shadow memory into state registers.
struct RegisterRange
{
    uint32 regOffset;   // Offset to the first register to load. Relative to the base address of the register type.
                        // E.g., PERSISTENT_SPACE_START for SH registers, etc.
    uint32 regCount;    // Number of registers to load.
};

// Defines a register and its associated offset and value.
struct RegisterValuePair
{
    uint32 offset; // Offset to the register to load.  Relative to the base address of the register type.
                   // E.g., PERSISTENT_SPACE_START for SH registers, etc.
    uint32 value;  // Register data to write
};

// =====================================================================================================================
// Defines two registers and their associated offsets and values.
struct PackedRegisterPair
{
    struct
    {
        uint32 offset0 : 16; // Offset to the register. Relative to the base address of the register type.
        uint32 offset1 : 16; // Offset to the register. Relative to the base address of the register type.
    };

    uint32 value0; // Register data to write for offset0.
    uint32 value1; // Register data to write for offset1.
};

// Struct to track packedRegPair lookup indexes
typedef struct
{
    uint32 lastSetVal;
    uint8 regIndex;

} UserDataEntryLookup;

constexpr uint32 MaxUserEntryLookupSetVal = UINT32_MAX;

// =====================================================================================================================
// Sets an offset and value in a packed register pair.
template <uint32 RegSpace>
static void SetOneRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              regAddr,
    uint32              value)
{
    const uint32        regPairIndex = (*pIndex) / 2;
    PackedRegisterPair* pRegPair     = &pRegPairs[regPairIndex];

    if (((*pIndex) % 2) == 0)
    {
        pRegPair->offset0 = regAddr - RegSpace;
        pRegPair->value0  = value;
    }
    else
    {
        pRegPair->offset1 = regAddr - RegSpace;
        pRegPair->value1  = value;
    }

    *pIndex = *pIndex + 1;
}

// =====================================================================================================================
// Sets offsets and values for a sequence of consecutive registers in packed register pairs.
template <uint32 RegSpace>
static void SetSeqRegValPairPacked(
    PackedRegisterPair* pRegPairs,
    uint32*             pIndex,
    uint16              startAddr,
    uint16              endAddr,
    const void*         pValues)
{
    const uint32* pUints = reinterpret_cast<const uint32*>(pValues);

    for (uint32 i = 0; i < endAddr - startAddr + 1; i++)
    {
        SetOneRegValPairPacked<RegSpace>(pRegPairs, pIndex, i + startAddr, pUints[i]);
    }
}

// Defines an invalid index for entries into a packed register pair lookup.
constexpr uint32 InvalidRegPairLookupIndex = 0xFF;

// =====================================================================================================================
// Sets the offset and value of a user-data entry in a packed register pair and updates the associated lookup table.
template <uint32 RegSpace>
static void SetOnePackedRegPairLookup(
    const uint32         regAddr,
    const uint16         baseRegAddr,
    const uint32         value,
    PackedRegisterPair*  pRegPairs,
    UserDataEntryLookup* pRegLookup,
    uint32               minLookupValue,
    uint32*              pNumRegs)
{
    uint32       regIndex     = *pNumRegs;             // Index into the regpair array
    const uint16 lookupIndex  = regAddr - baseRegAddr; // Index into the the reg lookup

    PAL_ASSERT((minLookupValue > 0) && (minLookupValue < MaxUserEntryLookupSetVal));

    // If this value hasn't been set since we last invalidated the data, set it now.
    if (pRegLookup[lookupIndex].lastSetVal < minLookupValue)
    {
        pRegLookup[lookupIndex].lastSetVal = minLookupValue;
        pRegLookup[lookupIndex].regIndex   = regIndex;
        (*pNumRegs)++;
    }
    else
    {
        // This shouldn't ever have a larger value.
        PAL_ASSERT(pRegLookup[lookupIndex].lastSetVal == minLookupValue);
        regIndex = pRegLookup[lookupIndex].regIndex;
    }

    SetOneRegValPairPacked<RegSpace>(pRegPairs, &regIndex, regAddr, value);
}

// =====================================================================================================================
// Sets offsets and values for a sequence of consecutive registers in packed register pairs and updates the associated
// lookup table.
template <uint32 RegSpace>
static void SetSeqPackedRegPairLookup(
    const uint32         startAddr,
    const uint32         endAddr,
    const uint16         baseRegAddr,
    const void*          pValues,
    PackedRegisterPair*  pRegPairs,
    UserDataEntryLookup* pRegLookup,
    uint32               minLookupValue,
    uint32*              pNumRegs)
{
    const uint32* pUints = reinterpret_cast<const uint32*>(pValues);

    for (uint32 i = 0; i < endAddr - startAddr + 1; i++)
    {
        SetOnePackedRegPairLookup<RegSpace>(i + startAddr,
                                            baseRegAddr,
                                            pUints[i],
                                            pRegPairs,
                                            pRegLookup,
                                            minLookupValue,
                                            pNumRegs);
    }
}

// Represents the maximum number of slots in the array of reference counters in the device, used for counting number
// of gfx cmd buffers that skipped a fast clear eliminate blit and the image that it was skipped for. The counter
// is stored with the device as images and command buffers can have separate lifetimes.
constexpr uint32 MaxNumFastClearImageRefs = 256;

// Represents the initial state of the reference counter that tracks the number of fast clear eliminates that were
// skipped in command buffers. When an image acquires a counter from the device, it sets it to an initial value of
// 1 (InUse).
enum RefCounterState : uint32
{
    Free  = 0,
    InUse = 1
};

// Common enums used by HWL settings
enum CsSimdDestCntlMode : uint32
{
    CsSimdDestCntlDefault = 0,
    CsSimdDestCntlForce1  = 1,
    CsSimdDestCntlForce0  = 2,
};

// Common enums used by HWL settings
enum ColorTransformValue : uint32
{
    ColorTransformAuto    = 0,
    ColorTransformDisable = 1,
};

enum PrefetchMethod : uint32
{
    PrefetchDisabled   = 0,
    PrefetchCpDma      = 1,
    PrefetchPrimeUtcL2 = 2,
};

enum DecompressMask : uint32
{
    DecompressDcc = 0x00000001,
    DecompressHtile = 0x00000002,
    DecompressFmask = 0x00000004,
    DecompressFastClear = 0x00000008,
};

enum OutOfOrderPrimMode : uint32
{
    OutOfOrderPrimDisable = 0,
    OutOfOrderPrimSafe = 1,
    OutOfOrderPrimAggressive = 2,
    OutOfOrderPrimAlways = 3,
};

// =====================================================================================================================
// A helper function to check that a series of addresses and struct-offsets are sequential.
// This is intended for use with static_asserts to ensure register layouts don't go out-of-sync.
struct CheckedRegPair {
    uint32 regOffset;
    size_t structOffset;
};
template <size_t N>
constexpr bool CheckSequentialRegs(
    const CheckedRegPair (&args)[N])
{
    uint32 regOffsets[N]    = {};
    size_t structOffsets[N] = {};
    for (int i = 0; i < N; i++)
    {
        regOffsets[i]    = args[i].regOffset;
        structOffsets[i] = args[i].structOffset;
    }
    return (Util::CheckSequential(regOffsets) && Util::CheckSequential(structOffsets, sizeof(uint32)));
}

// =====================================================================================================================
// Abstract class for accessing a Device's hardware-specific functionality common to all GFXIP hardware layers.
class GfxDevice
{
public:
    static constexpr bool ForceStateShadowing = false;

    // Destroys the GfxDevice object without freeing the system memory the object occupies.
    void Destroy() { this->~GfxDevice(); }

    virtual Result EarlyInit() = 0;
    virtual Result LateInit() = 0;
    virtual Result Finalize();
    virtual Result Cleanup();

    virtual Result HwlValidateImageViewInfo(const ImageViewInfo& viewInfo) const { return Result::Success; }
    virtual Result HwlValidateSamplerInfo(const SamplerInfo& samplerInfo)  const { return Result::Success; }

    Result InitHwlSettings(PalSettings* pSettings);

    const PalSettings& CoreSettings() const;

    virtual Result InitSettings() const = 0;
    virtual Util::MetroHash::Hash GetSettingsHash() const = 0;
    virtual void HwlValidateSettings(PalSettings* pSettings) = 0;
    virtual void HwlOverrideDefaultSettings(PalSettings* pSettings) = 0;
    virtual void HwlRereadSettings() = 0;
    virtual void HwlReadSettings() {}

    // This gives the GFX device an opportunity to override and/or fixup some of the PAL device properties after all
    // settings have been read. Called during IDevice::CommitSettingsAndInit().
    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const;

    virtual Result GetLinearImageAlignments(LinearImageAlignments* pAlignments) const = 0;

    virtual void BindTrapHandler(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset);
    virtual void BindTrapBuffer(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset);
    const BoundGpuMemory& TrapHandler(PipelineBindPoint pipelineType) const
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapHandler : m_computeTrapHandler; }
    const BoundGpuMemory& TrapBuffer(PipelineBindPoint pipelineType) const
        { return (pipelineType == PipelineBindPoint::Graphics) ? m_graphicsTrapBuffer : m_computeTrapBuffer; }

    uint32 QueueContextUpdateCounter();

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) = 0;

    virtual Result CreateDummyCommandStream(EngineType engineType, Pal::CmdStream** ppCmdStream) const = 0;

    // Determines the amount of storage needed for a QueueContext object for the given Queue type and ID. For Queue
    // types not supported by GFXIP hardware blocks, this should return zero.
    virtual size_t GetQueueContextSize(const QueueCreateInfo& createInfo) const = 0;

    // Constructs a new QueueContext object in preallocated memory for the specified parent Queue. This should always
    // fail with Result::ErrorUnavailable when called on a Queue which GFXIP hardware blocks don't support.
    virtual Result CreateQueueContext(
        const QueueCreateInfo& createInfo,
        Engine*                pEngine,
        void*                  pPlacementAddr,
        QueueContext**         ppQueueContext) = 0;

    virtual size_t GetComputePipelineSize(
        const ComputePipelineCreateInfo& createInfo,
        Result*                          pResult) const = 0;
    virtual Result CreateComputePipeline(
        const ComputePipelineCreateInfo& createInfo,
        void*                            pPlacementAddr,
        bool                             isInternal,
        IPipeline**                      ppPipeline) = 0;
    Result CreateComputePipelineInternal(
        const ComputePipelineCreateInfo& createInfo,
        ComputePipeline**                ppPipeline,
        Util::SystemAllocType            allocType);

   virtual size_t GetShaderLibrarySize(
        const ShaderLibraryCreateInfo&  createInfo,
        Result*                         pResult) const = 0;
    virtual Result CreateShaderLibrary(
        const ShaderLibraryCreateInfo&  createInfo,
        void*                           pPlacementAddr,
        bool                            isInternal,
        IShaderLibrary**                ppPipeline) = 0;

    virtual size_t GetGraphicsPipelineSize(
        const GraphicsPipelineCreateInfo& createInfo,
        bool                              isInternal,
        Result*                           pResult) const = 0;
    virtual Result CreateGraphicsPipeline(
        const GraphicsPipelineCreateInfo&         createInfo,
        const GraphicsPipelineInternalCreateInfo& internalInfo,
        void*                                     pPlacementAddr,
        bool                                      isInternal,
        IPipeline**                               ppPipeline) = 0;
    Result CreateGraphicsPipelineInternal(
        const GraphicsPipelineCreateInfo&         createInfo,
        const GraphicsPipelineInternalCreateInfo& internalInfo,
        GraphicsPipeline**                        ppPipeline,
        Util::SystemAllocType                     allocType);

    virtual bool DetermineHwStereoRenderingSupported(
        const GraphicPipelineViewInstancingInfo& viewInstancingInfo) const
        { return false; }

    virtual size_t GetColorBlendStateSize() const = 0;
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const = 0;
    Result CreateColorBlendStateInternal(
        const ColorBlendStateCreateInfo& createInfo,
        ColorBlendState**                ppColorBlendState,
        Util::SystemAllocType            allocType) const;
    void DestroyColorBlendStateInternal(
        ColorBlendState* pColorBlendState) const;

    virtual size_t GetDepthStencilStateSize() const = 0;
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const = 0;
    Result CreateDepthStencilStateInternal(
        const DepthStencilStateCreateInfo& createInfo,
        DepthStencilState**                ppDepthStencilState,
        Util::SystemAllocType              allocType) const;
    void DestroyDepthStencilStateInternal(
        DepthStencilState* pDepthStencilState) const;

    virtual size_t GetMsaaStateSize() const = 0;
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const = 0;
    Result CreateMsaaStateInternal(
        const MsaaStateCreateInfo& createInfo,
        MsaaState**                ppMsaaState,
        Util::SystemAllocType      allocType) const;
    void DestroyMsaaStateInternal(
        MsaaState* pMsaaState) const;
    virtual size_t GetImageSize(const ImageCreateInfo& createInfo) const = 0;
    virtual void CreateImage(
        Pal::Image* pParentImage,
        ImageInfo*  pImageInfo,
        void*       pPlacementAddr,
        GfxImage**  ppImage) const = 0;

    virtual size_t GetBorderColorPaletteSize(const BorderColorPaletteCreateInfo& createInfo, Result* pResult) const = 0;
    virtual Result CreateBorderColorPalette(
        const BorderColorPaletteCreateInfo& createInfo,
        void*                               pPlacementAddr,
        IBorderColorPalette**               ppBorderColorPalette) const = 0;

    virtual size_t GetQueryPoolSize(
        const QueryPoolCreateInfo& createInfo,
        Result*                    pResult) const = 0;
    virtual Result CreateQueryPool(
        const QueryPoolCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IQueryPool**               ppQueryPool) const = 0;

    virtual size_t GetCmdBufferSize(
        const CmdBufferCreateInfo& createInfo) const = 0;
    virtual Result CreateCmdBuffer(
        const CmdBufferCreateInfo& createInfo,
        void*                      pPlacementAddr,
        CmdBuffer**                ppCmdBuffer) = 0;

    virtual size_t GetIndirectCmdGeneratorSize(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        Result*                               pResult) const = 0;

    virtual Result CreateIndirectCmdGenerator(
        const IndirectCmdGeneratorCreateInfo& createInfo,
        void*                                 pPlacementAddr,
        IIndirectCmdGenerator**               ppGenerator) const = 0;

    virtual size_t GetColorTargetViewSize(
        Result* pResult) const = 0;
    virtual Result CreateColorTargetView(
        const ColorTargetViewCreateInfo&  createInfo,
        ColorTargetViewInternalCreateInfo internalInfo,
        void*                             pPlacementAddr,
        IColorTargetView**                ppColorTargetView) const = 0;

    virtual size_t GetDepthStencilViewSize(
        Result* pResult) const = 0;
    virtual Result CreateDepthStencilView(
        const DepthStencilViewCreateInfo&         createInfo,
        const DepthStencilViewInternalCreateInfo& internalInfo,
        void*                                     pPlacementAddr,
        IDepthStencilView**                       ppDepthStencilView) const = 0;

    virtual size_t GetPerfExperimentSize(
        const PerfExperimentCreateInfo& createInfo,
        Result*                         pResult) const = 0;

    virtual Result CreatePerfExperiment(
        const PerfExperimentCreateInfo& createInfo,
        void*                           pPlacementAddr,
        IPerfExperiment**               ppPerfExperiment) const = 0;

    virtual bool SupportsIterate256() const { return false; }

    virtual Result CreateCmdUploadRingInternal(
        const CmdUploadRingCreateInfo& createInfo,
        CmdUploadRing**                ppCmdUploadRing) = 0;

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const = 0;

    static bool IsImageFormatOverrideNeeded(
        ChNumFormat* pFormat,
        uint32*      pPixelsPerBlock);

    virtual void IncreaseMsaaHistogram(uint32 samples) {  }
    virtual void DecreaseMsaaHistogram(uint32 samples) {  }
    virtual bool UpdateSppState(const IImage& presentableImage) { return true; }
    virtual uint32 GetPixelCount() const { return 0; }
    virtual uint32 GetMsaaRate() const { return 0; }

    Pal::Device* Parent() const { return m_pParent; }
    Platform* GetPlatform() const;

    const RsrcProcMgr& RsrcProcMgr() const { return *m_pRsrcProcMgr; }

    virtual Result SetSamplePatternPalette(const SamplePatternPalette& palette) = 0;

    // Helper function that disables a specific CU mask within the UMD managed range.
    uint16 GetCuEnableMask(uint16 disabledCuMmask, uint32 enabledCuMaskSetting) const;

    // Helper function telling what kind of DCC format encoding an image created with
    // the specified creation image and all of its potential view formats will end up with
    virtual DccFormatEncoding ComputeDccFormatEncoding(
        const SwizzledFormat& swizzledFormat,
        const SwizzledFormat* pViewFormats,
        uint32                viewFormatCount) const = 0;

    // Init and get the cmd buffer that increment memory of frame count and write to register.
    Result InitAndGetFrameCountCmdBuffer(QueueType queueType, EngineType engineType, GfxCmdBuffer** ppBuffer);

    // Helper to check if this Device can support launching a CE preamble command stream with every Universal Queue
    // submission.
    bool SupportsCePreamblePerSubmit() const;

    void DescribeDispatch(
        GfxCmdBuffer*               pCmdBuf,
        RgpMarkerSubQueueFlags      subQueueFlags,
        Developer::DrawDispatchType cmdType,
        DispatchDims                offset,
        DispatchDims                launchSize,
        DispatchDims                logicalSize) const;

    void DescribeDraw(
        GfxCmdBuffer*               pCmdBuf,
        RgpMarkerSubQueueFlags      subQueueFlags,
        Developer::DrawDispatchType cmdType,
        uint32                      firstVertexUserDataIdx,
        uint32                      instanceOffsetUserDataIdx,
        uint32                      drawIndexUserDataIdx) const;

    void DescribeBindPipeline(
        GfxCmdBuffer*     pCmdBuf,
        const IPipeline*  pPipeline,
        uint64            apiPsoHash,
        PipelineBindPoint bindPoint) const;

#if PAL_DEVELOPER_BUILD
    void DescribeDrawDispatchValidation(
        GfxCmdBuffer* pCmdBuf,
        size_t        userDataCmdSize,
        size_t        miscCmdSize) const;

    void DescribeBindPipelineValidation(
        GfxCmdBuffer* pCmdBuf,
        size_t        pipelineCmdSize) const;

    void DescribeHotRegisters(
        GfxCmdBuffer* pCmdBuf,
        const uint32* pShRegSeenSets,
        const uint32* pShRegKeptSets,
        uint32        shRegCount,
        uint16        shRegBase,
        const uint32* pCtxRegSeenSets,
        const uint32* pCtxRegKeptSets,
        uint32        ctxRegCount,
        uint16        ctxRegBase) const;
#endif

#if DEBUG
    virtual uint32* TemporarilyHangTheGpu(
        EngineType engineType,
        uint32     number,
        uint32*    pCmdSpace) const
    {
        PAL_NEVER_CALLED();
        return nullptr;
    }
#endif

    virtual void PatchPipelineInternalSrdTable(
        void*       pDstSrdTable,
        const void* pSrcSrdTable,
        size_t      tableBytes,
        gpusize     dataGpuVirtAddr) const = 0;
    uint32* AllocateFceRefCount();

    virtual uint32 GetVarBlockSize() const { return 0; }

    virtual void InitAddrLibChipId(ADDR_CREATE_INPUT*  pInput) const;

    bool CanEnableDualSourceBlend(const ColorBlendStateCreateInfo& createInfo) const;

    static const MsaaQuadSamplePattern DefaultSamplePattern[];

    static uint32 VertsPerPrimitive(
        PrimitiveTopology topology,
        uint32            patchControlPoints);

    static uint32 VertsPerPrimitive(PrimitiveTopology topology);

    void DescribeBarrier(
        GfxCmdBuffer*                 pCmdBuf,
        const BarrierTransition*      pTransition,
        Developer::BarrierOperations* pOperations) const;

    void DescribeBarrierStart(GfxCmdBuffer* pCmdBuf, uint32 reason, Developer::BarrierType type) const;
    void DescribeBarrierEnd(GfxCmdBuffer* pCmdBuf, Developer::BarrierOperations* pOperations) const;

    virtual ClearMethod GetDefaultSlowClearMethod(
        const ImageCreateInfo&  createInfo,
        const SwizzledFormat&   clearFormat) const;

    virtual void DisableImageViewSrdEdgeClamp(uint32 count, void* pImageSrds) const {   }

    virtual bool DisableAc01ClearCodes() const { return true; };

    virtual void UpdateDisplayDcc(
        GfxCmdBuffer*                   pCmdBuf,
        const CmdPostProcessFrameInfo&  postProcessInfo,
        bool*                           pAddedGpuWork) const
    {
        PAL_ASSERT_ALWAYS();
    }

    PipelineLoader* GetPipelineLoader() { return &m_pipelineLoader; }

protected:
    static void FixupDecodedSrdFormat(
        const SwizzledFormat& imageFormat,
        SwizzledFormat*       pSrdFormat)
    {
        PAL_ASSERT(imageFormat.format != ChNumFormat::Undefined);
        if (imageFormat.format == ChNumFormat::A8_Unorm)
        {
            PAL_ASSERT(pSrdFormat->format == ChNumFormat::X8_Unorm);
            PAL_ASSERT((pSrdFormat->swizzle.swizzle[3] == ChannelSwizzle::X) &&
                       (imageFormat.swizzle.swizzle[3] == ChannelSwizzle::X));
            // It is only allowed to create A8_Unorm SRDs on A8_Unorm images;
            // fixup the decoded format to be consistent with that:
            pSrdFormat->format = ChNumFormat::A8_Unorm;
        }
    }

    uint32 GetCuEnableMaskInternal(uint32 disabledCuMmask, uint32 enabledCuMaskSetting) const;

    explicit GfxDevice(Device* pDevice, Pal::RsrcProcMgr* pRsrcProcMgr);
    virtual ~GfxDevice();

    static bool IsValidTypedBufferView(const BufferViewInfo& info);

    Device*const             m_pParent;
    Pal::RsrcProcMgr*        m_pRsrcProcMgr;
    DevDriver::SettingsBase* m_pDdSettingsLoader;

    PAL_ALIGN(32) uint32 m_fastClearImageRefs[MaxNumFastClearImageRefs];

#if DEBUG
    // Sometimes it is useful to temporarily hang the GPU during debugging to dump command buffers, etc.  This piece of
    // memory is used as a global location we can wait on using a WAIT_REG_MEM packet.  We only include this in debug
    // builds because we don't want to add extra overhead to release drivers.
    BoundGpuMemory m_debugStallGpuMem;
#endif

    // Store GPU memory and offsets for compute/graphics trap handlers and trap buffers.  Trap handlers are client-
    // installed hardware shaders that can be executed based on exceptions occurring in the main shader or in other
    // situations like supporting a debugger.  Trap buffers are just scratch memory that can be accessed from a trap
    // handler.
    BoundGpuMemory m_computeTrapHandler;
    BoundGpuMemory m_computeTrapBuffer;
    BoundGpuMemory m_graphicsTrapHandler;
    BoundGpuMemory m_graphicsTrapBuffer;

    Util::Mutex       m_queueContextUpdateLock;

    // Keep a watermark for sample-pos palette updates to the queue context. When a QueueContext pre-processes a submit, it
    // will check its watermark against the one owned by the device and update accordingly.
    // Access to this object must be serialized using m_queueContextUpdateLock.
    volatile uint32   m_queueContextUpdateCounter;

    PipelineLoader  m_pipelineLoader;

private:
    PAL_DISALLOW_DEFAULT_CTOR(GfxDevice);
    PAL_DISALLOW_COPY_AND_ASSIGN(GfxDevice);
};

// NOTE: Below are prototypes for several utility functions for each GFXIP namespace in PAL. These functions act as
// factories for creating GfxDevice objects for a specific hardware layer. Each GFXIP namespace must export the
// following functions:
//
// size_t GetDeviceSize();
// * This function returns the size in bytes needed for a GfxDevice object associated with a Pal::Device object.
//
// Result CreateDevice(
//      Device*                  pDevice,
//      void*                    pPlacementAddr,
//      DeviceInterfacePfnTable* pFnTable,
//      GfxDevice**              ppGfxDevice);
// * This function is the actual factory for creating GfxDevice objects. It creates a new object in the specified
//   preallocated memory buffer and returns a pointer to that object through ppGfxDevice.

namespace Gfx9
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*                   pDevice,
    void*                     pPlacementAddr,
    DeviceInterfacePfnTable*  pPfnTable,
    GfxDevice**               ppGfxDevice);
// Creates SettingsLoader object for Gfx9/10 hardware layer
extern DevDriver::SettingsBase* CreateSettingsLoader(Pal::Device* pDevice);
} // Gfx9

} // Pal

