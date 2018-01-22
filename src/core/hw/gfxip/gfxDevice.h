/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/cmdStream.h"
#include "core/platform.h"
#include "palHashMap.h"

typedef union _ADDR_CREATE_FLAGS ADDR_CREATE_FLAGS;
typedef struct _ADDR_REGISTER_VALUE ADDR_REGISTER_VALUE;

namespace Util { enum SystemAllocType : uint32; }
namespace Util { class File; }
namespace Util { class FileView; }
namespace Util { class Mutex; }

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
struct     DeviceInterfacePfnTable;
struct     FmaskViewInfo;
struct     GpuChipProperties;
struct     GraphicsPipelineCreateInfo;
struct     ImageCreateInfo;
struct     ImageInfo;
struct     ImageViewInfo;
struct     MsaaStateCreateInfo;
struct     PerfExperimentCreateInfo;
struct     QueryPoolCreateInfo;
struct     RasterStateCreateInfo;
struct     SamplerInfo;
struct     ScShaderMem;
struct     ScissorStateCreateInfo;
struct     ShaderCreateInfo;
struct     ViewportStateCreateInfo;

enum class PipelineBindPoint : uint32;
enum class ShaderType : uint32;

// Additional information for creating PAL-internal color target views.
struct ColorTargetViewInternalCreateInfo
{
    union
    {
        struct
        {
            uint32 dccDecompress    : 1;  // Indicates this color target view is for a DCC decompress
            uint32 fastClearElim    : 1;  // Indicates this color target view is for a fast-clear-eliminate
            uint32 fmaskDecompess   : 1;  // Indicates this color target view is for a fmask decompress
            uint32 depthStencilCopy : 1;  // Indicates this color target view is for a depth/stencil copy
            uint32 reserved         : 28; // Reserved for future use
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
            uint32 isExpand       : 1;  // true if setting up an expand operation
            uint32 isResummarize  : 1;  // true if setting up a hiz resummarize operation
            uint32 isDepthClear   : 1;  // true if this is a fast-depth clear
            uint32 isStencilClear : 1;  // true if this is a fast-stencil clear
            uint32 isDepthCopy    : 1;  // true if this is a depth copy
            uint32 isStencilCopy  : 1;  // true if this is a stencil copy
            uint32 reserved       : 26; // reserved, set to zero
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
            uint32 fastClearElim    :  1; // Fast clear eliminate BLT.
            uint32 fmaskDecompress  :  1; // FMask decompress BLT.
            uint32 dccDecompress    :  1; // DCC decompress BLT.
            uint32 resolveFixedFunc :  1; // Fixed function resolve.
            uint32 reserved         : 28;
        };
        uint32 u32All;
    } flags;
};

// Additional information for creating PAL-internal compound state.
struct CompoundStateInternalCreateInfo
{
    GraphicsPipelineInternalCreateInfo gfxPipelineInfo;
};

// Structure describing a single FLGL register command
struct FlglRegCmd
{
    uint32 offset;  // Register offset
    uint32 andMask; // AND mask
    uint32 orMask;  // OR mask
};

// Structure describing FLGL register sequence
constexpr uint32 FlglMaxRegseqCount = 6; // Magic number 6 from UGL

struct FlglRegSeq
{
    uint32     regSequenceCount;                // number of commands in sequence
    FlglRegCmd regSequence[FlglMaxRegseqCount]; // actual register commands
};

// Enumeration defining FLGL register sequence types
enum FlglRegSeqType
{
    FlglRegSeqSwapreadySet        = 0,
    FlglRegSeqSwapreadyReset      = 1,
    FlglRegSeqSwapreadyRead       = 2,
    FlglRegSeqSwaprequestSet      = 3,
    FlglRegSeqSwaprequestReset    = 4,
    FlglRegSeqSwaprequestRead     = 5,
    FlglRegSeqSwaprequestReadLow  = 6,
    FlglRegSeqMax
};

enum LateAllocVsMode : uint32
{
    LateAllocVsDisable = 0x00000000,
    LateAllocVsInvalid = 0xFFFFFFFF,

};

enum SmallPrimFilterCntl : uint32
{
    SmallPrimFilterDisable = 0x00000000,
    SmallPrimFilterEnablePoint = 0x00000001,
    SmallPrimFilterEnableLine = 0x00000002,
    SmallPrimFilterEnableTriangle = 0x00000004,
    SmallPrimFilterEnableRectangle = 0x00000008,
    SmallPrimFilterEnableAll = 0x0000000F,

};

// =====================================================================================================================
// Abstract class for accessing a Device's hardware-specific functionality common to all GFXIP hardware layers.
class GfxDevice
{
public:
    static constexpr bool ForceStateShadowing = false;

    // Destroys the GfxDevice object without freeing the system memory the object occupies.
    void Destroy() { this->~GfxDevice(); }

    virtual Result EarlyInit() = 0;
    virtual Result LateInit();
    virtual Result Finalize();
    virtual Result Cleanup();

    // This gives the GFX device an opportunity to override and/or fixup some of the PAL device properties after all
    // settings have been read. Called during IDevice::CommitSettingsAndInit().
    virtual void FinalizeChipProperties(GpuChipProperties* pChipProperties) const;

    virtual Result GetLinearImageAlignments(LinearImageAlignments* pAlignments) const = 0;

    virtual void BindTrapHandler(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) = 0;
    virtual void BindTrapBuffer(PipelineBindPoint pipelineType, IGpuMemory* pGpuMemory, gpusize offset) = 0;
    virtual const BoundGpuMemory& TrapHandler(PipelineBindPoint pipelineType) const = 0;
    virtual const BoundGpuMemory& TrapBuffer(PipelineBindPoint pipelineType) const  = 0;

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 339
    virtual Result InitMsaaQuadSamplePatternGpuMemory(
        IGpuMemory*                  pGpuMemory,
        gpusize                      memOffset,
        uint32                       numSamplesPerPixel,
        const MsaaQuadSamplePattern& quadSamplePattern) = 0;
#endif

    virtual Result CreateEngine(
        EngineType engineType,
        uint32     engineIndex,
        Engine**   ppEngine) = 0;

    // Determines the amount of storage needed for a QueueContext object for the given Queue type and ID. For Queue
    // types not supported by GFXIP hardware blocks, this should return zero.
    virtual size_t GetQueueContextSize(const QueueCreateInfo& createInfo) const = 0;

    // Constructs a new QueueContext object in preallocated memory for the specified parent Queue. This should always
    // fail with Result::ErrorUnavailable when called on a Queue which GFXIP hardware blocks don't support.
    virtual Result CreateQueueContext(
        Queue*         pQueue,
        Engine*        pEngine,
        void*          pPlacementAddr,
        QueueContext** ppQueueContext) = 0;

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

    virtual size_t GetColorBlendStateSize(const ColorBlendStateCreateInfo& createInfo, Result* pResult) const = 0;
    virtual Result CreateColorBlendState(
        const ColorBlendStateCreateInfo& createInfo,
        void*                            pPlacementAddr,
        IColorBlendState**               ppColorBlendState) const = 0;
    Result CreateColorBlendStateInternal(
        const ColorBlendStateCreateInfo& createInfo,
        ColorBlendState**                ppColorBlendState,
        Util::SystemAllocType            allocType) const;

    virtual size_t GetDepthStencilStateSize(
        const DepthStencilStateCreateInfo& createInfo,
        Result*                            pResult) const = 0;
    virtual Result CreateDepthStencilState(
        const DepthStencilStateCreateInfo& createInfo,
        void*                              pPlacementAddr,
        IDepthStencilState**               ppDepthStencilState) const = 0;
    Result CreateDepthStencilStateInternal(
        const DepthStencilStateCreateInfo& createInfo,
        DepthStencilState**                ppDepthStencilState,
        Util::SystemAllocType              allocType) const;

    virtual size_t GetMsaaStateSize(
        const MsaaStateCreateInfo& createInfo,
        Result*                    pResult) const = 0;
    virtual Result CreateMsaaState(
        const MsaaStateCreateInfo& createInfo,
        void*                      pPlacementAddr,
        IMsaaState**               ppMsaaState) const = 0;
    Result CreateMsaaStateInternal(
        const MsaaStateCreateInfo& createInfo,
        MsaaState**                ppMsaaState,
        Util::SystemAllocType      allocType) const;
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
        const ColorTargetViewCreateInfo&         createInfo,
        const ColorTargetViewInternalCreateInfo& internalInfo,
        void*                                    pPlacementAddr,
        IColorTargetView**                       ppColorTargetView) const = 0;

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

    virtual Result CreateCmdUploadRingInternal(
        const CmdUploadRingCreateInfo& createInfo,
        CmdUploadRing**                ppCmdUploadRing) = 0;

    virtual Result InitAddrLibCreateInput(
        ADDR_CREATE_FLAGS*   pCreateFlags,
        ADDR_REGISTER_VALUE* pRegValue) const = 0;

    virtual bool IsImageFormatOverrideNeeded(
        const ImageCreateInfo& imageCreateInfo,
        ChNumFormat*           pFormat,
        uint32*                pPixelsPerBlock) const = 0;

    Pal::Device* Parent() const;
    Platform* GetPlatform() const;

    const RsrcProcMgr& RsrcProcMgr() const { return *m_pRsrcProcMgr; }

    const BoundGpuMemory& CeRingBufferGpuMem(bool isNested) const
        { return m_ceRingBufferGpuMem[static_cast<uint32>(isNested)]; }

    virtual Result SetSamplePatternPalette(const SamplePatternPalette& palette) = 0;

    virtual uint32 GetValidFormatFeatureFlags(
        const ChNumFormat format,
        const ImageAspect aspect,
        const ImageTiling tiling) const = 0;

    // Helper function that disables a specific CU mask within the UMD managed range.
    uint16 GetCuEnableMask(uint16 disabledCuMmask, uint32 enabledCuMaskSetting) const;

    // Helper function telling whether an image created with the specified creation image has all of its
    // potential view formats compatible with DCC.
    virtual bool AreImageFormatsDccCompatible(const ImageCreateInfo& imageCreateInfo) const = 0;

    // Init and get the cmd buffer that increment memory of frame count and write to register.
    Result InitAndGetFrameCountCmdBuffer(QueueType queueType, EngineType engineType, GfxCmdBuffer** ppBuffer);

    void SetFlglRegisterSequence(const FlglRegSeq& regSeq, uint32 index) { m_flglRegSeq[index] = regSeq; }

    const FlglRegSeq* GetFlglRegisterSequence(uint32 index) const { return &m_flglRegSeq[index]; }

    // Helper to check if this Device can support launching a CE preamble command stream with every Universal Queue
    // submission.
    bool SupportsCePreamblePerSubmit() const;

    void DescribeDispatch(
        GfxCmdBuffer*               pCmdBuf,
        Developer::DrawDispatchType cmdType,
        uint32                      xOffset,
        uint32                      yOffset,
        uint32                      zOffset,
        uint32                      xDim,
        uint32                      yDim,
        uint32                      zDim) const;

    void DescribeDraw(
        GfxCmdBuffer*               pCmdBuf,
        Developer::DrawDispatchType cmdType,
        uint32                      firstVertexUserDataIdx,
        uint32                      instanceOffsetUserDataIdx,
        uint32                      drawIndexUserDataIdx) const;

    virtual Result P2pBltWaModifyRegionListMemory(
        const IGpuMemory&       dstGpuMemory,
        uint32                  regionCount,
        const MemoryCopyRegion* pRegions,
        uint32*                 pNewRegionCount,
        MemoryCopyRegion*       pNewRegions,
        gpusize*                pChunkAddrs) const
    {
        PAL_NEVER_CALLED();
        return Result::Success;
    }

    virtual Result P2pBltWaModifyRegionListImage(
        const Pal::Image&      srcImage,
        const Pal::Image&      dstImage,
        uint32                 regionCount,
        const ImageCopyRegion* pRegions,
        uint32*                pNewRegionCount,
        ImageCopyRegion*       pNewRegions,
        gpusize*               pChunkAddrs) const
    {
        PAL_NEVER_CALLED();
        return Result::Success;
    }

    virtual Result P2pBltWaModifyRegionListImageToMemory(
        const Pal::Image&            srcImage,
        const IGpuMemory&            dstGpuMemory,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const
    {
        PAL_NEVER_CALLED();
        return Result::Success;
    }

    virtual Result P2pBltWaModifyRegionListMemoryToImage(
        const IGpuMemory&            srcGpuMemory,
        const Pal::Image&            dstImage,
        uint32                       regionCount,
        const MemoryImageCopyRegion* pRegions,
        uint32*                      pNewRegionCount,
        MemoryImageCopyRegion*       pNewRegions,
        gpusize*                     pChunkAddrs) const
    {
        PAL_NEVER_CALLED();
        return Result::Success;
    }

    virtual bool UsesIndexedLoad() const
    {
        PAL_NEVER_CALLED();
        return true;
    }

    bool   UseFixedLateAllocVsLimit() const { return m_useFixedLateAllocVsLimit; }
    uint32 LateAllocVsLimit() const { return m_lateAllocVsLimit; }

    uint32 GetSmallPrimFilter() const { return m_smallPrimFilter; }
    bool WaEnableDccCacheFlushAndInvalidate() const { return m_waEnableDccCacheFlushAndInvalidate; }
    bool WaTcCompatZRange() const { return m_waTcCompatZRange; }
    bool DegeneratePrimFilter() const { return m_degeneratePrimFilter; }

protected:
    uint32 GetCuEnableMaskInternal(uint32 disabledCuMmask, uint32 enabledCuMaskSetting) const;

    explicit GfxDevice(Device* pDevice, Pal::RsrcProcMgr* pRsrcProcMgr, uint32 frameCountRegOffset);
    virtual ~GfxDevice();

    Result AllocateCeRingBufferGpuMem(
        gpusize sizeInBytes,
        gpusize alignment);

    Device*const            m_pParent;
    Pal::RsrcProcMgr*       m_pRsrcProcMgr;
    FlglRegSeq              m_flglRegSeq[FlglRegSeqMax]; // Holder for FLGL sync register sequences

#if DEBUG
    // Sometimes it is useful to temporarily hang the GPU during debugging to dump command buffers, etc.  This piece of
    // memory is used as a global location we can wait on using a WAIT_REG_MEM packet.  We only include this in debug
    // builds because we don't want to add extra overhead to release drivers.
    BoundGpuMemory  m_debugStallGpuMem;
#endif

    // Memory of the frame count, will be incremented and written to register when doing present.
    BoundGpuMemory m_frameCountGpuMem;

    // Command buffers for submitting the frame count. One buffer for each queue type.
    // Will be initialized when we get the buffer for the first time.
    GfxCmdBuffer* m_pFrameCountCmdBuffer[QueueType::QueueTypeCount];

    // Offset of the register to write frame count. Will be 0 if the register is not supported.
    const uint32  m_frameCntReg;

    bool m_useFixedLateAllocVsLimit;
    uint32 m_lateAllocVsLimit;
    uint32 m_smallPrimFilter;
    bool m_waEnableDccCacheFlushAndInvalidate;
    bool m_waTcCompatZRange;
    bool m_degeneratePrimFilter;

private:
    // GPU memory which backs the ring buffer used by the CE for dumping CE RAM before draws and dispatches. There are
    // two copies of the ring, one for root-level command buffers and another for nested command buffers.
    BoundGpuMemory  m_ceRingBufferGpuMem[2];

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

#if PAL_BUILD_GFX6
namespace Gfx6
{
extern size_t GetDeviceSize();
extern Result CreateDevice(
    Device*                  pDevice,
    void*                    pPlacementAddr,
    DeviceInterfacePfnTable* pPfnTable,
    GfxDevice**              ppGfxDevice);
} // Gfx6
#endif

#if PAL_BUILD_GFX9
namespace Gfx9
{
extern size_t GetDeviceSize(GfxIpLevel  gfxLevel);
extern Result CreateDevice(
    Device*                   pDevice,
    void*                     pPlacementAddr,
    DeviceInterfacePfnTable*  pPfnTable,
    GfxDevice**               ppGfxDevice);
} // Gfx9
#endif

} // Pal
