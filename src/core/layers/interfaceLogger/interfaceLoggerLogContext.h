/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_DEVELOPER_BUILD

#include "core/layers/decorators.h"
#include "palFile.h"
#include "palJsonWriter.h"

namespace Pal
{
namespace InterfaceLogger
{

class Platform;

// An enumeration that represents each PAL interface class.
enum class InterfaceObject : uint32
{
    BorderColorPalette = 0,
    CmdAllocator,
    CmdBuffer,
    ColorBlendState,
    ColorTargetView,
    DepthStencilState,
    DepthStencilView,
    Device,
    Fence,
    GpuEvent,
    GpuMemory,
    Image,
    IndirectCmdGenerator,
    MsaaState,
    Pipeline,
    Platform,
    PrivateScreen,
    QueryPool,
    Queue,
    QueueSemaphore,
    Screen,
    ShaderLibrary,
    SwapChain,
    Count
};

// An enumeration that represents each PAL interface function.
enum class InterfaceFunc : uint32
{
    BorderColorPaletteUpdate = 0,
    BorderColorPaletteBindGpuMemory,
    BorderColorPaletteDestroy,
    CmdAllocatorReset,
    CmdAllocatorTrim,
    CmdAllocatorDestroy,
    CmdBufferBegin,
    CmdBufferEnd,
    CmdBufferReset,
    CmdBufferCmdBindPipeline,
    CmdBufferCmdPrimeGpuCaches,
    CmdBufferCmdBindMsaaState,
    CmdBufferCmdSaveGraphicsState,
    CmdBufferCmdRestoreGraphicsState,
    CmdBufferCmdBindColorBlendState,
    CmdBufferCmdBindDepthStencilState,
    CmdBufferCmdSetDepthBounds,
    CmdBufferCmdSetUserData,
    CmdBufferCmdDuplicateUserData,
    CmdBufferCmdSetKernelArguments,
    CmdBufferCmdSetVertexBuffers,
    CmdBufferCmdBindIndexData,
    CmdBufferCmdBindTargets,
    CmdBufferCmdBindStreamOutTargets,
    CmdBufferCmdSetPerDrawVrsRate,
    CmdBufferCmdSetVrsCenterState,
    CmdBufferCmdBindSampleRateImage,
    CmdBufferCmdResolvePrtPlusImage,
    CmdBufferCmdSetBlendConst,
    CmdBufferCmdSetInputAssemblyState,
    CmdBufferCmdSetTriangleRasterState,
    CmdBufferCmdSetPointLineRasterState,
    CmdBufferCmdSetLineStippleState,
    CmdBufferCmdSetDepthBiasState,
    CmdBufferCmdSetStencilRefMasks,
    CmdBufferCmdSetUserClipPlanes,
    CmdBufferCmdSetMsaaQuadSamplePattern,
    CmdBufferCmdSetViewports,
    CmdBufferCmdSetScissorRects,
    CmdBufferCmdSetGlobalScissor,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    CmdBufferCmdSetColorWriteMask,
    CmdBufferCmdSetRasterizerDiscardEnable,
#endif
    CmdBufferCmdBarrier,
    CmdBufferCmdRelease,
    CmdBufferCmdAcquire,
    CmdBufferCmdReleaseEvent,
    CmdBufferCmdAcquireEvent,
    CmdBufferCmdReleaseThenAcquire,
    CmdBufferCmdDraw,
    CmdBufferCmdDrawOpaque,
    CmdBufferCmdDrawIndexed,
    CmdBufferCmdDrawIndirectMulti,
    CmdBufferCmdDrawIndexedIndirectMulti,
    CmdBufferCmdDispatch,
    CmdBufferCmdDispatchIndirect,
    CmdBufferCmdDispatchOffset,
    CmdBufferCmdDispatchDynamic,
    CmdBufferCmdDispatchMesh,
    CmdBufferCmdDispatchMeshIndirectMulti,
    CmdBufferCmdCopyMemory,
    CmdBufferCmdCopyMemoryByGpuVa,
    CmdBufferCmdCopyImage,
    CmdBufferCmdCopyMemoryToImage,
    CmdBufferCmdCopyImageToMemory,
    CmdBufferCmdCopyMemoryToTiledImage,
    CmdBufferCmdCopyTiledImageToMemory,
    CmdBufferCmdCopyTypedBuffer,
    CmdBufferCmdCopyRegisterToMemory,
    CmdBufferCmdScaledCopyImage,
    CmdBufferCmdGenerateMipmaps,
    CmdBufferCmdColorSpaceConversionCopy,
    CmdBufferCmdCloneImageData,
    CmdBufferCmdUpdateMemory,
    CmdBufferCmdUpdateBusAddressableMemoryMarker,
    CmdBufferCmdFillMemory,
    CmdBufferCmdClearColorBuffer,
    CmdBufferCmdClearBoundColorTargets,
    CmdBufferCmdClearColorImage,
    CmdBufferCmdClearBoundDepthStencilTargets,
    CmdBufferCmdClearDepthStencil,
    CmdBufferCmdClearBufferView,
    CmdBufferCmdClearImageView,
    CmdBufferCmdResolveImage,
    CmdBufferCmdSetEvent,
    CmdBufferCmdResetEvent,
    CmdBufferCmdPredicateEvent,
    CmdBufferCmdMemoryAtomic,
    CmdBufferCmdBeginQuery,
    CmdBufferCmdEndQuery,
    CmdBufferCmdResolveQuery,
    CmdBufferCmdResetQueryPool,
    CmdBufferCmdWriteTimestamp,
    CmdBufferCmdWriteImmediate,
    CmdBufferCmdLoadBufferFilledSizes,
    CmdBufferCmdSaveBufferFilledSizes,
    CmdBufferCmdSetBufferFilledSize,
    CmdBufferCmdBindBorderColorPalette,
    CmdBufferCmdSetPredication,
    CmdBufferCmdSuspendPredication,
    CmdBufferCmdIf,
    CmdBufferCmdElse,
    CmdBufferCmdEndIf,
    CmdBufferCmdWhile,
    CmdBufferCmdEndWhile,
    CmdBufferCmdWaitRegisterValue,
    CmdBufferCmdWaitMemoryValue,
    CmdBufferCmdWaitBusAddressableMemoryMarker,
    CmdBufferCmdLoadCeRam,
    CmdBufferCmdDumpCeRam,
    CmdBufferCmdWriteCeRam,
    CmdBufferCmdAllocateEmbeddedData,
    CmdBufferCmdExecuteNestedCmdBuffers,
    CmdBufferCmdSaveComputeState,
    CmdBufferCmdRestoreComputeState,
    CmdBufferCmdExecuteIndirectCmds,
    CmdBufferCmdSetMarker,
    CmdBufferCmdPresent,
    CmdBufferCmdCommentString,
    CmdBufferCmdNop,
    CmdBufferCmdXdmaWaitFlipPending,
    CmdBufferCmdStartGpuProfilerLogging,
    CmdBufferCmdStopGpuProfilerLogging,
    CmdBufferDestroy,
    CmdBufferCmdSetViewInstanceMask,
    CmdUpdateHiSPretests,
    CmdBufferCmdSetClipRects,
    CmdBufferCmdPostProcessFrame,
    ColorBlendStateDestroy,
    DepthStencilStateDestroy,
    DeviceCommitSettingsAndInit,
    DeviceFinalize,
    DeviceCleanup,
    DeviceSetMaxQueuedFrames,
    DeviceAddGpuMemoryReferences,
    DeviceRemoveGpuMemoryReferences,
    DeviceSetClockMode,
    DeviceSetMgpuMode,
    DeviceOfferAllocations,
    DeviceReclaimAllocations,
    DeviceResetFences,
    DeviceWaitForFences,
    DeviceBindTrapHandler,
    DeviceBindTrapBuffer,
    DeviceCreateQueue,
    DeviceCreateMultiQueue,
    DeviceCreateGpuMemory,
    DeviceCreatePinnedGpuMemory,
    DeviceCreateSvmGpuMemory,
    DeviceOpenSharedGpuMemory,
    DeviceOpenExternalSharedGpuMemory,
    DeviceOpenPeerGpuMemory,
    DeviceCreateImage,
    DeviceCreatePresentableImage,
    DeviceOpenPeerImage,
    DeviceOpenExternalSharedImage,
    DeviceCreateColorTargetView,
    DeviceCreateDepthStencilView,
    DeviceCreateTypedBufferViewSrds,
    DeviceCreateUntypedBufferViewSrds,
    DeviceCreateImageViewSrds,
    DeviceCreateFmaskViewSrds,
    DeviceCreateSamplerSrds,
    DeviceCreateBvhSrds,
    DeviceSetSamplePatternPalette,
    DeviceCreateBorderColorPalette,
    DeviceCreateComputePipeline,
    DeviceCreateGraphicsPipeline,
    DeviceCreateShaderLibrary,
    DeviceCreateMsaaState,
    DeviceCreateColorBlendState,
    DeviceCreateDepthStencilState,
    DeviceCreateQueueSemaphore,
    DeviceOpenSharedQueueSemaphore,
    DeviceOpenExternalSharedQueueSemaphore,
    DeviceCreateFence,
    DeviceOpenFence,
    DeviceCreateGpuEvent,
    DeviceCreateQueryPool,
    DeviceCreateCmdAllocator,
    DeviceCreateCmdBuffer,
    DeviceCreateIndirectCmdGenerator,
    DeviceGetPrivateScreens,
    DeviceAddEmulatedPrivateScreen,
    DeviceRemoveEmulatedPrivateScreen,
    DeviceCreatePrivateScreenImage,
    DeviceCreateSwapChain,
    DeviceSetPowerProfile,
    DeviceFlglQueryState,
    DeviceFlglSetSyncConfiguration,
    DeviceFlglGetSyncConfiguration,
    DeviceFlglSetFrameLock,
    DeviceFlglSetGenLock,
    DeviceFlglResetFrameCounter,
    DeviceFlglGetFrameCounter,
    DeviceFlglGetFrameCounterResetStatus,
    DeviceCreateVirtualDisplay,
    DeviceDestroyVirtualDisplay,
    DeviceGetVirtualDisplayProperties,
    FenceDestroy,
    GpuEventSet,
    GpuEventReset,
    GpuEventBindGpuMemory,
    GpuEventDestroy,
    GpuMemorySetPriority,
    GpuMemoryMap,
    GpuMemoryUnmap,
    GpuMemorySetSdiRemoteBusAddress,
    GpuMemoryDestroy,
    ImageBindGpuMemory,
    ImageDestroy,
    IndirectCmdGeneratorBindGpuMemory,
    IndirectCmdGeneratorDestroy,
    MsaaStateDestroy,
    PipelineCreateLaunchDescriptor,
    PipelineLinkWithLibraries,
    PipelineDestroy,
    PlatformEnumerateDevices,
    PlatformGetScreens,
    PlatformTurboSyncControl,
    PlatformDestroy,
    PrivateScreenEnable,
    PrivateScreenDisable,
    PrivateScreenBlank,
    PrivateScreenPresent,
    PrivateScreenSetGammaRamp,
    PrivateScreenSetPowerMode,
    PrivateScreenSetDisplayMode,
    PrivateScreenSetColorMatrix,
    PrivateScreenSetEventAfterVsync,
    PrivateScreenEnableAudio,
    QueryPoolBindGpuMemory,
    QueryPoolDestroy,
    QueryPoolReset,
    QueueSubmit,
    QueueWaitIdle,
    QueueSignalQueueSemaphore,
    QueueWaitQueueSemaphore,
    QueuePresentDirect,
    QueuePresentSwapChain,
    QueueDelay,
    QueueDelayAfterVsync,
    QueueRemapVirtualMemoryPages,
    QueueCopyVirtualMemoryPageMappings,
    QueueAssociateFenceWithLastSubmit,
    QueueSetExecutionPriority,
    QueueDestroy,
    QueueSemaphoreDestroy,
    ScreenIsImplicitFullscreenOwnershipSafe,
    ScreenQueryCurrentDisplayMode,
    ScreenTakeFullscreenOwnership,
    ScreenReleaseFullscreenOwnership,
    ScreenSetGammaRamp,
    ScreenWaitForVerticalBlank,
    ScreenDestroy,
    ShaderLibraryDestroy,
    SwapChainAcquireNextImage,
    SwapChainWaitIdle,
    SwapChainDestroy,
    Count
};

// Must be provided to each LogContext::BeginFunc call.
struct BeginFuncInfo
{
    InterfaceFunc funcId;       // Which function will be logged.
    uint32        objectId;     // The PAL interface object being called.
    uint64        preCallTime;  // The tick immediately before calling down to the next layer.
    uint64        postCallTime; // The tick immediately after calling down to the next layer.
};

// =====================================================================================================================
// JSON stream that records the text stream using a staging buffer and a log file. WriteFile must be called explicitly
// to flush all buffered text. Note that this makes it possible to generate JSON text before OpenFile has been called.
class LogStream final : public Util::JsonStream
{
public:
    explicit LogStream(Platform* pPlatform);
    virtual ~LogStream();

    Result OpenFile(const char* pFilePath);
    Result WriteFile();

    // Returns true if the log file has already been opened.
    bool IsFileOpen() const { return m_file.IsOpen(); }

    virtual void WriteString(const char* pString, uint32 length) override;
    virtual void WriteCharacter(char character) override;

private:
    void VerifyUnusedSpace(uint32 size);

    Platform*const m_pPlatform;
    Util::File     m_file;       // The text stream is being written here.
    char*          m_pBuffer;    // Buffered text data that needs to be written to the file.
    uint32         m_bufferSize; // The size of the buffer in characters.
    uint32         m_bufferUsed; // How many characters of the buffer are in use.

    PAL_DISALLOW_DEFAULT_CTOR(LogStream);
    PAL_DISALLOW_COPY_AND_ASSIGN(LogStream);
};

// =====================================================================================================================
// A logging context contains all state needed to write a single log file. It also wraps a JSON writer with PAL-specific
// helper functions. This keeps the JSON output consistent, making it easier to parse written logs in external tools.
//
// At the highest level, the JSON stream contains a list of maps, where each map is an entry in the log. Each entry
// contains a "_type" key whose value is a string indicating what type of entry is being parsed. This key exists solely
// as a hint to external tools. This layer will use the following types with the given keys (order not guaranteed).
//
// "Platform": Contains basic information about the platform that captured the log.
// Required Keys
//  - "api": Which PAL client API was making the PAL calls.
//  - "os": Which operating system was in use.
//  - "timerFreq": The number of CPU timer ticks per second. Useful for interpreting the timer values in this log.
//  - "createInfo": A map containing the client's PlatformCreateInfo.
//
// "LogFile": Names a companion JSON log file to the current JSON stream. The companion log may have capture data in
// parallel to the current stream.
// Required Keys
//  - "name": The name of the companion log relative to the logging directory.
//
// "BeginElevatedLogging"/"EndElevatedLogging": Indicates that the elevated logging mode was enabled or disabled. These
// entries are only written into the main log but include the current time for comparison against companion logs.
// Required Keys
// - "time": The time on the platform's timer immediately after the mode switch.
//
// "InterfaceFunc": Represents a single PAL interface function call, listing its inputs, outputs, and other useful info.
// Required Keys
//  - "this": The PAL object that was called.
//  - "name": The name of the interface function (e.g., CreateCmdBuffer).
//  - "thread": The thread that called this function. This layer generates zero-based, user-friendly thread IDs.
//  - "preCallTime": The time on the platform's timer immediately before this function was called.
//  - "postCallTime": The time on the platform's timer immediately after this function was called.
// Optional Keys
//  - "input": A map containing all logged inputs of this function.
//  - "output": A map containing all logged outputs of this function.
//
// Note that the LogContext also defines a common format for logging instances of PAL interface objects. Each object is
// represented by a map containing a "class" key identifying the PAL interface class (e.g., IDevice) and an "id" key
// identifying the particular instance of the class. All IDs are unique and zero-based.
class LogContext : public Util::JsonWriter
{
public:
    explicit LogContext(Platform* pPlatform);
    virtual ~LogContext();

    // Must be called once to associate a context with a log file. Logging can occur before the log is opened.
    Result OpenFile(const char* pFilePath) { return m_stream.OpenFile(pFilePath); }

    // These functions begin and end a specially formatted map which represents a PAL interface function.
    void BeginFunc(const BeginFuncInfo& info, uint32 threadId);
    void EndFunc();

    // Most PAL functions need to log inputs and outputs. They should be placed into maps using these functions.
    void BeginInput() { KeyAndBeginMap("input", false); }
    void EndInput()   { EndMap(); }

    void BeginOutput() { KeyAndBeginMap("output", false); }
    void EndOutput()   { EndMap(); }

    // These functions create a map that represents a particular InterfaceLogger decorated PAL object.
    void Object(const IBorderColorPalette* pDecorator);
    void Object(const ICmdAllocator* pDecorator);
    void Object(const ICmdBuffer* pDecorator);
    void Object(const IColorBlendState* pDecorator);
    void Object(const IColorTargetView* pDecorator);
    void Object(const IDepthStencilState* pDecorator);
    void Object(const IDepthStencilView* pDecorator);
    void Object(const IDevice* pDecorator);
    void Object(const IFence* pDecorator);
    void Object(const IGpuEvent* pDecorator);
    void Object(const IGpuMemory* pDecorator);
    void Object(const IImage* pDecorator);
    void Object(const IIndirectCmdGenerator* pDecorator);
    void Object(const IMsaaState* pDecorator);
    void Object(const IPipeline* pDecorator);
    void Object(const IPrivateScreen* pDecorator);
    void Object(const IQueryPool* pDecorator);
    void Object(const IQueue* pDecorator);
    void Object(const IQueueSemaphore* pDecorator);
    void Object(const IScreen* pDecorator);
    void Object(const IShaderLibrary* pDecorator);
    void Object(const ISwapChain* pDecorator);

    // These functions create a list or map that represents a PAL interface structure.
    void Struct(const AcquireNextImageInfo& value);
    void Struct(const BarrierInfo& value);
    void Struct(const PrimeGpuCacheRange& value);
    void Struct(const AcquireReleaseInfo& value);
    void Struct(const BindStreamOutTargetParams& value);
    void Struct(const BindTargetParams& value);
    void Struct(const BlendConstParams& value);
    void Struct(const BorderColorPaletteCreateInfo& value);
    void Struct(const BoundColorTarget& value);
    void Struct(Box value);
    void Struct(const BufferViewInfo& value);
    void Struct(ChannelMapping value);
    void Struct(const ClearBoundTargetRegion& value);
    void Struct(const ClearColor& value);
    void Struct(const CmdAllocatorCreateInfo& value);
    void Struct(const CmdBufferBuildInfo& value);
    void Struct(const CmdBufferCreateInfo& value);
    void Struct(const CmdBufInfo& value);
    void Struct(const CmdPostProcessFrameInfo& value);
    void Struct(const ColorBlendStateCreateInfo& value);
    void Struct(const ColorKey& value);
    void Struct(const ColorSpaceConversionRegion& value);
    void Struct(const ColorSpaceConversionTable& value);
    void Struct(const ColorTargetViewCreateInfo& value);
    void Struct(const ColorTransform& value);
    void Struct(const ComputePipelineCreateInfo& value);
    void Struct(const DepthBiasParams& value);
    void Struct(const DepthBoundsParams& value);
    void Struct(const DepthStencilSelectFlags& value);
    void Struct(const DepthStencilStateCreateInfo& value);
    void Struct(const DepthStencilViewCreateInfo& value);
    void Struct(const DeviceFinalizeInfo& value);
    void Struct(const DirectCaptureInfo& value);

    void Struct(DispatchDims value);
    void Struct(const DoppDesktopInfo& value);
    void Struct(const DoppRef& value);
    void Struct(const DynamicComputeShaderInfo& value);
    void Struct(const DynamicGraphicsShaderInfo& value);
    void Struct(const DynamicGraphicsShaderInfos& value);
    void Struct(const DynamicGraphicsState& value);
    void Struct(Extent2d value);
    void Struct(Extent3d value);
    void Struct(const ExternalGpuMemoryOpenInfo& value);
    void Struct(const ExternalImageOpenInfo& value);
    void Struct(const ExternalQueueSemaphoreOpenInfo& value);
    void Struct(const ExternalResourceOpenInfo& value);
    void Struct(const FlglState& value);
    void Struct(const GlSyncConfig& value);
    void Struct(const FmaskViewInfo& value);
    void Struct(const FullScreenFrameMetadataControlFlags& value);
    void Struct(const GammaRamp& value);
    void Struct(const GlobalScissorParams& value);
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    void Struct(const ColorWriteMaskParams& value);
#endif
    void Struct(const GpuEventCreateInfo& value);
    void Struct(GpuMemoryCreateFlags value);
    void Struct(const GpuMemoryCreateInfo& value);
    void Struct(const GpuMemoryOpenInfo& value);
    void Struct(const GpuMemoryRef& value);
    void Struct(const GraphicsPipelineCreateInfo& value);
    void Struct(const CpuVirtAddrAndStride& value);
    void Struct(const GpuVirtAddrAndStride& value);
    void Struct(const HiSPretests& value);
    void Struct(const ImageCopyRegion& value);
    void Struct(ImageCreateFlags value);
    void Struct(const ImageCreateInfo& value);
    void Struct(ImageLayout value);
    void Struct(const ImageResolveRegion& value);
    void Struct(const ImageScaledCopyRegion& value);
    void Struct(ImageUsageFlags value);
    void Struct(const ImageViewInfo& value);
    void Struct(const IndirectCmdGeneratorCreateInfo& value);
    void Struct(const InheritedStateParams& value);
    void Struct(const InputAssemblyStateParams& value);
    void Struct(const MemoryCopyRegion& value);
    void Struct(const MemoryImageCopyRegion& value);
    void Struct(const MemoryTiledImageCopyRegion& value);
    void Struct(const MsaaQuadSamplePattern& value);
    void Struct(const MsaaStateCreateInfo& value);
    void Struct(Offset2d value);
    void Struct(Offset3d value);
    void Struct(const PeerGpuMemoryOpenInfo& value);
    void Struct(const PeerImageOpenInfo& value);
    void Struct(const PerSubQueueSubmitInfo& value);
    void Struct(const PinnedGpuMemoryCreateInfo& value);
    void Struct(const PipelineBindParams& value);
    void Struct(LibraryCreateFlags value);
    void Struct(PipelineCreateFlags value);
    void Struct(const PlatformCreateInfo& value);
    void Struct(const PointLineRasterStateParams& value);
    void Struct(const LineStippleStateParams& value);
    void Struct(const PowerSwitchInfo& value);
    void Struct(const PresentableImageCreateInfo& value);
    void Struct(const PresentDirectInfo& value);
    void Struct(const PresentSwapChainInfo& value);
    void Struct(const PrtPlusImageResolveRegion& value);
    void Struct(const PrivateDisplayMode& value);
    void Struct(const PrivateDisplayTiming& value);
    void Struct(const PrivateScreenCaps& value);
    void Struct(const PrivateScreenCreateInfo& value);
    void Struct(const PrivateScreenEnableInfo& value);
    void Struct(const PrivateScreenImageCreateInfo& value);
    void Struct(const PrivateScreenPresentInfo& value);
    void Struct(const PrivateScreenProperties& value);
    void Struct(const QueryControlFlags& value);
    void Struct(const QueryPoolCreateInfo& value);
    void Struct(const QueueCreateInfo& value);
    void Struct(const QueueSemaphoreCreateInfo& value);
    void Struct(const QueueSemaphoreOpenInfo& value);
    void Struct(Range value);
    void Struct(Rational value);
    void Struct(Rect value);
    void Struct(RgbFloat value);
    void Struct(const SamplePatternPalette& value);
    void Struct(const SamplerInfo& value);
    void Struct(const BvhInfo& value);
    void Struct(SamplePos value);
    void Struct(ScaledCopyFlags value);
    void Struct(const ScaledCopyInfo& value);
    void Struct(const GenMipmapsInfo& value);
    void Struct(const ScissorRectParams& value);
    void Struct(const SetClockModeInput& value);
    void Struct(const SetClockModeOutput& value);
    void Struct(const SetMgpuModeInput& value);
    void Struct(const ShaderLibraryCreateInfo& value);
    void Struct(const ShaderLibraryFunctionInfo& value);
    void Struct(SignedExtent2d value);
    void Struct(SignedExtent3d value);
    void Struct(const StencilRefMaskParams& value);
    void Struct(SubresId value);
    void Struct(SubresRange value);
    void Struct(const SvmGpuMemoryCreateInfo& value);
    void Struct(const SwapChainCreateInfo& value);
    void Struct(SwizzledFormat value);
    void Struct(TessDistributionFactors value);
    void Struct(TexFilter value);
    void Struct(const TriangleRasterStateParams& value);
    void Struct(const TurboSyncControlInput& value);
    void Struct(const TypedBufferCopyRegion& value);
    void Struct(const TypedBufferCreateInfo& value);
    void Struct(const TypedBufferInfo& value);
    void Struct(const UserClipPlane& value);
    void Struct(const ViewportParams& value);
    void Struct(const VirtualMemoryCopyPageMappingsRange& value);
    void Struct(const VirtualMemoryRemapRange& value);
    void Struct(const VirtualDisplayInfo& value);
    void Struct(const VirtualDisplayProperties& value);
    void Struct(const VrsCenterState&  centerState);
    void Struct(const VrsRateParams&  rateParams);
    void Struct(const FenceCreateInfo& value);
    void Struct(const FenceOpenInfo& value);
    void Struct(const GpuMemSubAllocInfo& value);

    // These functions create a string value for a PAL interface enumeration.
    void Enum(AtomicOp value);
    void Enum(Developer::BarrierReason value);
#if PAL_BUILD_GFX11
    void Enum(DispatchInterleaveSize value);
#endif
    void Enum(BinningOverride value);
    void Enum(Blend value);
    void Enum(BlendFunc value);
    void Enum(BorderColorType value);
    void Enum(ChNumFormat value);
    void Enum(ChannelSwizzle value);
    void Enum(ClearColorType value);
    void Enum(CompareFunc value);
    void Enum(CullMode value);
    void Enum(DepthRange value);
    void Enum(DepthClampMode value);
    void Enum(DeviceClockMode value);
    void Enum(EngineType value);
    void Enum(FaceOrientation value);
    void Enum(FillMode value);
    void Enum(FlglSupport value);
    void Enum(GpuHeap value);
    void Enum(GpuHeapAccess value);
    void Enum(GpuMemPriority value);
    void Enum(GpuMemPriorityOffset value);
    void Enum(PrtPlusResolveType value);
    void Enum(BoxSortHeuristic value);
    void Enum(HwPipePoint value);
    void Enum(ImageRotation value);
    void Enum(ImageTexOptLevel value);
    void Enum(ImageTiling value);
    void Enum(ImageTilingPattern value);
    void Enum(ImageType value);
    void Enum(ImageViewType value);
    void Enum(IndexType value);
    void Enum(IndirectParamType value);
    void Enum(LogicOp value);
    void Enum(MetadataMode value);
    void Enum(MgpuMode value);
    void Enum(MipFilter value);
    void Enum(NullGpuId value);
    void Enum(PipelineBindPoint value);
    void Enum(PointOrigin value);
    void Enum(PowerProfile value);
    void Enum(PredicateType value);
    void Enum(PresentMode value);
    void Enum(PrimitiveTopology value);
    void Enum(PrimitiveType value);
    void Enum(PrivateDisplayColorDepth value);
    void Enum(PrivateDisplayPixelEncoding value);
    void Enum(PrivateDisplayPowerState value);
    void Enum(PrivateScreenType value);
    void Enum(ProvokingVertex value);
    void Enum(PrtMapAccessType value);
    void Enum(PrtMapType value);
    void Enum(QueryPoolType value);
    void Enum(QueryType value);
    void Enum(QueuePriority value);
    void Enum(QueueType value);
    void Enum(ReclaimResult value);
    void Enum(ResolveMode value);
    void Enum(Result value);
    void Enum(ShadeMode value);
    void Enum(StencilOp value);
    void Enum(SubmitOptMode value);
    void Enum(SurfaceTransformFlags value);
    void Enum(SwapChainMode value);
    void Enum(TexAddressMode value);
    void Enum(TexFilterMode value);
    void Enum(TilingOptMode value);
    void Enum(VaRange value);
    void Enum(VirtualGpuMemAccessMode value);
    void Enum(VrsShadingRate value);
    void Enum(VrsCombiner value);
    void Enum(WsiPlatform value);
    void Enum(XyFilter value);
    void Enum(ZFilter value);
    void Enum(VirtualDisplayVSyncMode value);
    void Enum(ImmediateDataWidth value);
    void Enum(TurboSyncControlMode value);

    // These functions create a list that specifies all set flags. The function name indicates the PAL enum type.
    void CacheCoherencyUsageFlags(uint32 flags);
    void PipelineStageFlags(uint32 flags);
    void ComputeStateFlags(uint32 flags);
    void CopyControlFlags(uint32 flags);
    void GpuMemoryRefFlags(uint32 flags);
    void ImageLayoutEngineFlags(uint32 flags);
    void ImageLayoutUsageFlags(uint32 flags);
    void QueryPipelineStatsFlags(uint32 flags);
    void QueryResultFlags(uint32 flags);
    void ClearColorImageFlags(uint32 flags);
    void ClearDepthStencilFlags(uint32 flags);
    void ResolveImageFlags(uint32 flags);

    template <typename O>
    void KeyAndObject(const char* pKey, const O* pObj) { Key(pKey); Object(pObj); }

    template <typename E>
    void KeyAndEnum(const char* pKey, E value) { Key(pKey); Enum(value); }

    template <typename S>
    void KeyAndStruct(const char* pKey, const S& value) { Key(pKey); Struct(value); }

    void KeyAndCacheCoherencyUsageFlags(const char* pKey, uint32 flags) { Key(pKey); CacheCoherencyUsageFlags(flags); }
    void KeyAndPipelineStageFlags(const char* pKey, uint32 flags)       { Key(pKey); PipelineStageFlags(flags); }
    void KeyAndComputeStateFlags(const char* pKey, uint32 flags)        { Key(pKey); ComputeStateFlags(flags); }
    void KeyAndCopyControlFlags(const char* pKey, uint32 flags)         { Key(pKey); CopyControlFlags(flags); }
    void KeyAndGpuMemoryRefFlags(const char* pKey, uint32 flags)        { Key(pKey); GpuMemoryRefFlags(flags); }
    void KeyAndImageLayoutEngineFlags(const char* pKey, uint32 flags)   { Key(pKey); ImageLayoutEngineFlags(flags); }
    void KeyAndImageLayoutUsageFlags(const char* pKey, uint32 flags)    { Key(pKey); ImageLayoutUsageFlags(flags); }
    void KeyAndQueryPipelineStatsFlags(const char* pKey, uint32 flags)  { Key(pKey); QueryPipelineStatsFlags(flags); }
    void KeyAndQueryResultFlags(const char* pKey, uint32 flags)         { Key(pKey); QueryResultFlags(flags); }
    void KeyAndClearColorImageFlags(const char* pKey, uint32 flags)     { Key(pKey); ClearColorImageFlags(flags); }
    void KeyAndClearDepthStencilFlags(const char* pKey, uint32 flags)   { Key(pKey); ClearDepthStencilFlags(flags); }
    void KeyAndResolveImageFlags(const char* pKey, uint32 flags)        { Key(pKey); ResolveImageFlags(flags); }

    void KeyAndClientData(const char* pKey, void* pClientData)
        { Key(pKey); Value(reinterpret_cast<uintptr_t>(pClientData)); }

    // Unlike the other member functions, these helper functions return a human readable name for the given enum value
    // and do not modify the LogContext. They are intended to help label some PAL arrays that index using enums.
    static const char* GetQueueName(QueueType value);
    static const char* GetEngineName(EngineType value);
    static const char* GetVrsCenterRateName(VrsCenterRates value);
    static const char* GetVrsCombinerStageName(VrsCombinerStage value);

private:
    void Object(InterfaceObject objectType, uint32 objectId);

    LogStream m_stream;

    PAL_DISALLOW_DEFAULT_CTOR(LogContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(LogContext);
};

} // InterfaceLogger
} // Pal

#endif
