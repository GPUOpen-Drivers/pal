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

#if PAL_DEVELOPER_BUILD

#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerScreen.h"
#include "g_platformSettings.h"
#include "palSysUtil.h"
#include "palVectorImpl.h"
#include <ctime>

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

static constexpr uint32 GenCalls = LogFlagGeneralCalls;
static constexpr uint32 CrtDstry = LogFlagCreateDestroy;
static constexpr uint32 BindMem  = LogFlagBindGpuMemory;
static constexpr uint32 QueueOps = LogFlagQueueOps;
static constexpr uint32 CmdBuild = LogFlagCmdBuilding;
static constexpr uint32 CrtSrds  = LogFlagCreateSrds;

struct FuncLoggingTableEntry
{
    InterfaceFunc function;    // The interface function this entry represents.
    uint32        logFlagMask; // The mask of all LogFlag bits that apply to this function.
};

static constexpr FuncLoggingTableEntry FuncLoggingTable[] =
{
    { InterfaceFunc::BorderColorPaletteUpdate,                      (GenCalls)            },
    { InterfaceFunc::BorderColorPaletteBindGpuMemory,               (BindMem)             },
    { InterfaceFunc::BorderColorPaletteDestroy,                     (CrtDstry | BindMem)  },
    { InterfaceFunc::CmdAllocatorReset,                             (GenCalls | CmdBuild) },
    { InterfaceFunc::CmdAllocatorTrim,                              (GenCalls | CmdBuild) },
    { InterfaceFunc::CmdAllocatorDestroy,                           (CrtDstry | CmdBuild) },
    { InterfaceFunc::CmdBufferBegin,                                (CmdBuild)            },
    { InterfaceFunc::CmdBufferEnd,                                  (CmdBuild)            },
    { InterfaceFunc::CmdBufferReset,                                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindPipeline,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdPrimeGpuCaches,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindMsaaState,                     (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSaveGraphicsState,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdRestoreGraphicsState,              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindColorBlendState,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindDepthStencilState,             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetDepthBounds,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetUserData,                       (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDuplicateUserData,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetKernelArguments,                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetVertexBuffers,                  (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindIndexData,                     (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindTargets,                       (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindStreamOutTargets,              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetPerDrawVrsRate,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetVrsCenterState,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindSampleRateImage,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdResolvePrtPlusImage,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetBlendConst,                     (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetInputAssemblyState,             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetTriangleRasterState,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetPointLineRasterState,           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetLineStippleState,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetDepthBiasState,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetStencilRefMasks,                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetUserClipPlanes,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetMsaaQuadSamplePattern,          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetViewports,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetScissorRects,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetGlobalScissor,                  (CmdBuild)            },
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    { InterfaceFunc::CmdBufferCmdSetColorWriteMask,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetRasterizerDiscardEnable,        (CmdBuild)            },
#endif
    { InterfaceFunc::CmdBufferCmdBarrier,                           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdRelease,                           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdAcquire,                           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdReleaseEvent,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdAcquireEvent,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdReleaseThenAcquire,                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDraw,                              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDrawOpaque,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDrawIndexed,                       (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDrawIndirectMulti,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDrawIndexedIndirectMulti,          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatch,                          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatchIndirect,                  (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatchOffset,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatchDynamic,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatchMesh,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDispatchMeshIndirectMulti,         (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyMemory,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryByGpuVa,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyImage,                         (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToImage,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyImageToMemory,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToTiledImage,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyTiledImageToMemory,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyTypedBuffer,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCopyRegisterToMemory,              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdScaledCopyImage,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdGenerateMipmaps,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdColorSpaceConversionCopy,          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCloneImageData,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdUpdateMemory,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdUpdateBusAddressableMemoryMarker,  (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdFillMemory,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearColorBuffer,                  (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearBoundColorTargets,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearColorImage,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearBoundDepthStencilTargets,     (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearDepthStencil,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearBufferView,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdClearImageView,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdResolveImage,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetEvent,                          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdResetEvent,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdPredicateEvent,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdMemoryAtomic,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBeginQuery,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdEndQuery,                          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdResolveQuery,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdResetQueryPool,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWriteTimestamp,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWriteImmediate,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdLoadBufferFilledSizes,             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSaveBufferFilledSizes,             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetBufferFilledSize,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdBindBorderColorPalette,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetPredication,                    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSuspendPredication,                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdIf,                                (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdElse,                              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdEndIf,                             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWhile,                             (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdEndWhile,                          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWaitRegisterValue,                 (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWaitMemoryValue,                   (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWaitBusAddressableMemoryMarker,    (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdLoadCeRam,                         (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdDumpCeRam,                         (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdWriteCeRam,                        (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdAllocateEmbeddedData,              (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdExecuteNestedCmdBuffers,           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSaveComputeState,                  (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdRestoreComputeState,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdExecuteIndirectCmds,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetMarker,                         (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdPresent,                           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdCommentString,                     (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdNop,                               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdXdmaWaitFlipPending,               (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdStartGpuProfilerLogging,           (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdStopGpuProfilerLogging,            (CmdBuild)            },
    { InterfaceFunc::CmdBufferDestroy,                              (CrtDstry | CmdBuild) },
    { InterfaceFunc::CmdBufferCmdSetViewInstanceMask,               (CmdBuild)            },
    { InterfaceFunc::CmdUpdateHiSPretests,                          (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdSetClipRects,                      (CmdBuild)            },
    { InterfaceFunc::CmdBufferCmdPostProcessFrame,                  (CmdBuild)            },
    { InterfaceFunc::ColorBlendStateDestroy,                        (CrtDstry)            },
    { InterfaceFunc::DepthStencilStateDestroy,                      (CrtDstry)            },
    { InterfaceFunc::DeviceCommitSettingsAndInit,                   (GenCalls)            },
    { InterfaceFunc::DeviceFinalize,                                (GenCalls)            },
    { InterfaceFunc::DeviceCleanup,                                 (GenCalls)            },
    { InterfaceFunc::DeviceSetMaxQueuedFrames,                      (GenCalls | QueueOps) },
    { InterfaceFunc::DeviceAddGpuMemoryReferences,                  (GenCalls)            },
    { InterfaceFunc::DeviceRemoveGpuMemoryReferences,               (GenCalls)            },
    { InterfaceFunc::DeviceSetClockMode,                            (GenCalls)            },
    { InterfaceFunc::DeviceSetMgpuMode,                             (GenCalls)            },
    { InterfaceFunc::DeviceOfferAllocations,                        (GenCalls)            },
    { InterfaceFunc::DeviceReclaimAllocations,                      (GenCalls)            },
    { InterfaceFunc::DeviceResetFences,                             (GenCalls)            },
    { InterfaceFunc::DeviceWaitForFences,                           (GenCalls)            },
    { InterfaceFunc::DeviceBindTrapHandler,                         (GenCalls)            },
    { InterfaceFunc::DeviceBindTrapBuffer,                          (GenCalls)            },
    { InterfaceFunc::DeviceCreateQueue,                             (CrtDstry | QueueOps) },
    { InterfaceFunc::DeviceCreateMultiQueue,                        (CrtDstry | QueueOps) },
    { InterfaceFunc::DeviceCreateGpuMemory,                         (CrtDstry)            },
    { InterfaceFunc::DeviceCreatePinnedGpuMemory,                   (CrtDstry)            },
    { InterfaceFunc::DeviceCreateSvmGpuMemory,                      (CrtDstry)            },
    { InterfaceFunc::DeviceOpenSharedGpuMemory,                     (CrtDstry)            },
    { InterfaceFunc::DeviceOpenExternalSharedGpuMemory,             (CrtDstry)            },
    { InterfaceFunc::DeviceOpenPeerGpuMemory,                       (CrtDstry)            },
    { InterfaceFunc::DeviceCreateImage,                             (CrtDstry)            },
    { InterfaceFunc::DeviceCreatePresentableImage,                  (CrtDstry)            },
    { InterfaceFunc::DeviceOpenPeerImage,                           (CrtDstry)            },
    { InterfaceFunc::DeviceOpenExternalSharedImage,                 (CrtDstry)            },
    { InterfaceFunc::DeviceCreateColorTargetView,                   (CrtDstry)            },
    { InterfaceFunc::DeviceCreateDepthStencilView,                  (CrtDstry)            },
    { InterfaceFunc::DeviceCreateTypedBufferViewSrds,               (CrtSrds)             },
    { InterfaceFunc::DeviceCreateUntypedBufferViewSrds,             (CrtSrds)             },
    { InterfaceFunc::DeviceCreateImageViewSrds,                     (CrtSrds)             },
    { InterfaceFunc::DeviceCreateFmaskViewSrds,                     (CrtSrds)             },
    { InterfaceFunc::DeviceCreateSamplerSrds,                       (CrtSrds)             },
    { InterfaceFunc::DeviceCreateBvhSrds,                           (CrtSrds)             },
    { InterfaceFunc::DeviceSetSamplePatternPalette,                 (GenCalls)            },
    { InterfaceFunc::DeviceCreateBorderColorPalette,                (CrtDstry)            },
    { InterfaceFunc::DeviceCreateComputePipeline,                   (CrtDstry)            },
    { InterfaceFunc::DeviceCreateGraphicsPipeline,                  (CrtDstry)            },
    { InterfaceFunc::DeviceCreateShaderLibrary,                     (CrtDstry)            },
    { InterfaceFunc::DeviceCreateMsaaState,                         (CrtDstry)            },
    { InterfaceFunc::DeviceCreateColorBlendState,                   (CrtDstry)            },
    { InterfaceFunc::DeviceCreateDepthStencilState,                 (CrtDstry)            },
    { InterfaceFunc::DeviceCreateQueueSemaphore,                    (CrtDstry | QueueOps) },
    { InterfaceFunc::DeviceOpenSharedQueueSemaphore,                (CrtDstry | QueueOps) },
    { InterfaceFunc::DeviceOpenExternalSharedQueueSemaphore,        (CrtDstry | QueueOps) },
    { InterfaceFunc::DeviceCreateFence,                             (CrtDstry)            },
    { InterfaceFunc::DeviceOpenFence,                               (CrtDstry)            },
    { InterfaceFunc::DeviceCreateGpuEvent,                          (CrtDstry)            },
    { InterfaceFunc::DeviceCreateQueryPool,                         (CrtDstry)            },
    { InterfaceFunc::DeviceCreateCmdAllocator,                      (CrtDstry)            },
    { InterfaceFunc::DeviceCreateCmdBuffer,                         (CrtDstry)            },
    { InterfaceFunc::DeviceCreateIndirectCmdGenerator,              (CrtDstry)            },
    { InterfaceFunc::DeviceGetPrivateScreens,                       (CrtDstry)            },
    { InterfaceFunc::DeviceAddEmulatedPrivateScreen,                (CrtDstry)            },
    { InterfaceFunc::DeviceRemoveEmulatedPrivateScreen,             (CrtDstry)            },
    { InterfaceFunc::DeviceCreatePrivateScreenImage,                (CrtDstry)            },
    { InterfaceFunc::DeviceCreateSwapChain,                         (CrtDstry)            },
    { InterfaceFunc::DeviceSetPowerProfile,                         (GenCalls)            },
    { InterfaceFunc::DeviceFlglQueryState,                          (GenCalls)            },
    { InterfaceFunc::DeviceFlglSetSyncConfiguration,                (GenCalls)            },
    { InterfaceFunc::DeviceFlglGetSyncConfiguration,                (GenCalls)            },
    { InterfaceFunc::DeviceFlglSetFrameLock,                        (GenCalls)            },
    { InterfaceFunc::DeviceFlglSetGenLock,                          (GenCalls)            },
    { InterfaceFunc::DeviceFlglResetFrameCounter,                   (GenCalls)            },
    { InterfaceFunc::DeviceFlglGetFrameCounter,                     (GenCalls)            },
    { InterfaceFunc::DeviceFlglGetFrameCounterResetStatus,          (GenCalls)            },
    { InterfaceFunc::DeviceCreateVirtualDisplay,                    (CrtDstry)            },
    { InterfaceFunc::DeviceDestroyVirtualDisplay,                   (CrtDstry)            },
    { InterfaceFunc::DeviceGetVirtualDisplayProperties,             (GenCalls)            },
    { InterfaceFunc::FenceDestroy,                                  (CrtDstry)            },
    { InterfaceFunc::GpuEventSet,                                   (GenCalls)            },
    { InterfaceFunc::GpuEventReset,                                 (GenCalls)            },
    { InterfaceFunc::GpuEventBindGpuMemory,                         (BindMem)             },
    { InterfaceFunc::GpuEventDestroy,                               (CrtDstry | BindMem)  },
    { InterfaceFunc::GpuMemorySetPriority,                          (GenCalls)            },
    { InterfaceFunc::GpuMemoryMap,                                  (GenCalls)            },
    { InterfaceFunc::GpuMemoryUnmap,                                (GenCalls)            },
    { InterfaceFunc::GpuMemorySetSdiRemoteBusAddress,               (GenCalls)            },
    { InterfaceFunc::GpuMemoryDestroy,                              (CrtDstry | BindMem)  },
    { InterfaceFunc::ImageBindGpuMemory,                            (BindMem)             },
    { InterfaceFunc::ImageDestroy,                                  (CrtDstry | BindMem)  },
    { InterfaceFunc::IndirectCmdGeneratorBindGpuMemory,             (BindMem)             },
    { InterfaceFunc::IndirectCmdGeneratorDestroy,                   (CrtDstry | BindMem)  },
    { InterfaceFunc::MsaaStateDestroy,                              (CrtDstry)            },
    { InterfaceFunc::PipelineCreateLaunchDescriptor,                (GenCalls)            },
    { InterfaceFunc::PipelineLinkWithLibraries,                     (GenCalls)            },
    { InterfaceFunc::PipelineDestroy,                               (CrtDstry)            },
    { InterfaceFunc::PlatformEnumerateDevices,                      (GenCalls)            },
    { InterfaceFunc::PlatformGetScreens,                            (GenCalls)            },
    { InterfaceFunc::PlatformTurboSyncControl,                      (GenCalls)            },
    { InterfaceFunc::PlatformDestroy,                               (CrtDstry)            },
    { InterfaceFunc::PrivateScreenEnable,                           (GenCalls)            },
    { InterfaceFunc::PrivateScreenDisable,                          (GenCalls)            },
    { InterfaceFunc::PrivateScreenBlank,                            (GenCalls)            },
    { InterfaceFunc::PrivateScreenPresent,                          (GenCalls)            },
    { InterfaceFunc::PrivateScreenSetGammaRamp,                     (GenCalls)            },
    { InterfaceFunc::PrivateScreenSetPowerMode,                     (GenCalls)            },
    { InterfaceFunc::PrivateScreenSetDisplayMode,                   (GenCalls)            },
    { InterfaceFunc::PrivateScreenSetColorMatrix,                   (GenCalls)            },
    { InterfaceFunc::PrivateScreenSetEventAfterVsync,               (GenCalls)            },
    { InterfaceFunc::PrivateScreenEnableAudio,                      (GenCalls)            },
    { InterfaceFunc::QueryPoolBindGpuMemory,                        (BindMem)             },
    { InterfaceFunc::QueryPoolDestroy,                              (CrtDstry | BindMem)  },
    { InterfaceFunc::QueryPoolReset,                                (GenCalls)            },
    { InterfaceFunc::QueueSubmit,                                   (QueueOps)            },
    { InterfaceFunc::QueueWaitIdle,                                 (QueueOps)            },
    { InterfaceFunc::QueueSignalQueueSemaphore,                     (QueueOps)            },
    { InterfaceFunc::QueueWaitQueueSemaphore,                       (QueueOps)            },
    { InterfaceFunc::QueuePresentDirect,                            (QueueOps)            },
    { InterfaceFunc::QueuePresentSwapChain,                         (QueueOps)            },
    { InterfaceFunc::QueueDelay,                                    (QueueOps)            },
    { InterfaceFunc::QueueDelayAfterVsync,                          (QueueOps)            },
    { InterfaceFunc::QueueRemapVirtualMemoryPages,                  (QueueOps)            },
    { InterfaceFunc::QueueCopyVirtualMemoryPageMappings,            (QueueOps)            },
    { InterfaceFunc::QueueAssociateFenceWithLastSubmit,             (QueueOps)            },
    { InterfaceFunc::QueueSetExecutionPriority,                     (QueueOps)            },
    { InterfaceFunc::QueueDestroy,                                  (CrtDstry | QueueOps) },
    { InterfaceFunc::QueueSemaphoreDestroy,                         (CrtDstry | QueueOps) },
    { InterfaceFunc::ScreenIsImplicitFullscreenOwnershipSafe,       (GenCalls)            },
    { InterfaceFunc::ScreenQueryCurrentDisplayMode,                 (GenCalls)            },
    { InterfaceFunc::ScreenTakeFullscreenOwnership,                 (GenCalls)            },
    { InterfaceFunc::ScreenReleaseFullscreenOwnership,              (GenCalls)            },
    { InterfaceFunc::ScreenSetGammaRamp,                            (GenCalls)            },
    { InterfaceFunc::ScreenWaitForVerticalBlank,                    (GenCalls)            },
    { InterfaceFunc::ScreenDestroy,                                 (CrtDstry)            },
    { InterfaceFunc::ShaderLibraryDestroy,                          (CrtDstry)            },
    { InterfaceFunc::SwapChainAcquireNextImage,                     (GenCalls | QueueOps) },
    { InterfaceFunc::SwapChainWaitIdle,                             (GenCalls)            },
    { InterfaceFunc::SwapChainDestroy,                              (CrtDstry)            },
};

static_assert(ArrayLen(FuncLoggingTable) == static_cast<size_t>(InterfaceFunc::Count),
              "The FuncLoggingTable must be updated.");

// =====================================================================================================================
Platform::Platform(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled)
    :
    PlatformDecorator(createInfo, allocCb, InterfaceLoggerCb, enabled, enabled, pNextPlatform),
    m_createInfo(createInfo),
    m_pMainLog(nullptr),
    m_nextThreadId(0),
    m_objectId(0),
    m_activePreset(0),
    m_threadDataVec(this)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32 idx = 0; idx < static_cast<uint32>(InterfaceFunc::Count); ++idx)
    {
        PAL_ASSERT(static_cast<uint32>(FuncLoggingTable[idx].function) == idx);
    }
#endif

    m_flags.u32All = 0;

    memset(&m_startTime, 0, sizeof(m_startTime));
    memset(&m_threadKey, 0, sizeof(m_threadKey));

    // Default to log everything until we load settings from the first device.
    memset(&m_loggingPresets, 0xFF, sizeof(m_loggingPresets));

    // Initialize these to zero so that the first call to NewObjectId for each type will return zero.
    for (uint32 idx = 0; idx < static_cast<uint32>(InterfaceObject::Count); ++idx)
    {
        m_nextObjectIds[idx] = 0;
    }

    m_objectId = NewObjectId(InterfaceObject::Platform);
}

// =====================================================================================================================
Platform::~Platform()
{
    // Tear-down the GPUs first so that we don't try to log their Cleanup() calls later on.
    TearDownGpus();

    // Delete the thread key and all thread-specific data.
    if (m_flags.threadKeyCreated)
    {
        const Result result = DeleteThreadLocalKey(m_threadKey);
        PAL_ASSERT(result == Result::Success);
    }

    for (uint32 idx = 0; idx < m_threadDataVec.NumElements(); ++idx)
    {
        auto* pThreadData = m_threadDataVec.At(idx);

        PAL_SAFE_DELETE(pThreadData->pContext, this);
        PAL_SAFE_DELETE(pThreadData, this);
    }

    m_threadDataVec.Clear();

    PAL_SAFE_DELETE(m_pMainLog, this);

    // If someone manages to call a logging function after destruction this might protect us a bit.
    m_flags.threadKeyCreated  = 0;
    m_flags.multithreaded     = 0;
    m_flags.settingsCommitted = 0;
}

// =====================================================================================================================
Result Platform::Create(
    const PlatformCreateInfo&   createInfo,
    const Util::AllocCallbacks& allocCb,
    IPlatform*                  pNextPlatform,
    bool                        enabled,
    void*                       pPlacementAddr,
    IPlatform**                 ppPlatform)
{
    auto* pPlatform = PAL_PLACEMENT_NEW(pPlacementAddr) Platform(createInfo, allocCb, pNextPlatform, enabled);
    Result result   = pPlatform->Init();
    if (result == Result::Success)
    {
        (*ppPlatform) = pPlatform;
    }
    else
    {
        pPlatform->Destroy();
    }

    return result;
}

// =====================================================================================================================
Result Platform::Init()
{
    Result result = PlatformDecorator::Init();

    if (m_layerEnabled && (result == Result::Success))
    {
        // Create the key we will use to manage thread-specific data.
        result = CreateThreadLocalKey(&m_threadKey);
        m_flags.threadKeyCreated = (result == Result::Success);

        // Query the timer frequency and starting time.
        uint64 timerFreq = 0;

        if (result == Result::Success)
        {
#if   defined(__unix__)
            if (clock_gettime(CLOCK_MONOTONIC, &m_startTime) == -1)
            {
                result = Result::ErrorUnknown;
            }
            else
            {
                // The timer is always in units of nanoseconds.
                timerFreq = 1000 * 1000 * 1000;
            }
#endif
        }

        if (result == Result::Success)
        {
            // Note that we dynamically allocate the main log context because its constructor and destructor write
            // JSON which can trigger a dynamic memory allocation. If this layer isn't enabled, we shouldn't allocate
            // any memory aside from what we require to decorate the platform.
            m_pMainLog = PAL_NEW(LogContext, this, AllocInternal) (this);

            if (m_pMainLog == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            // Write an entry to the main log with some general platform information.
            m_pMainLog->BeginMap(false);
            m_pMainLog->KeyAndValue("_type", "Platform");
            m_pMainLog->KeyAndValue("api", GetClientApiStr());

#if   defined(__unix__)
            m_pMainLog->KeyAndValue("os", "Linux");
#else
            static_assert(0, "Unknown client OS.");
#endif

            m_pMainLog->KeyAndValue("timerFreq", timerFreq);
            m_pMainLog->KeyAndStruct("createInfo", m_createInfo);
            m_pMainLog->EndMap();
        }
    }

    return result;
}

// =====================================================================================================================
// This will initialize all logging directories and other logging settings the first time it is called.
Result Platform::CommitLoggingSettings()
{
    // It's not impossible for multiple threads to call this at the same time...
    MutexAuto lock(&m_platformMutex);

    Result result = Result::Success;

    if (m_flags.settingsCommitted == 0)
    {
        // Save a copy of the logging presets.
        const auto& settings = PlatformSettings();
        m_loggingPresets[0] = settings.interfaceLoggerConfig.basePreset;
        m_loggingPresets[1] = settings.interfaceLoggerConfig.elevatedPreset;

        // Try to create the root log directory.
        result = CreateLogDir(settings.interfaceLoggerConfig.logDirectory);

        if (result == Result::Success)
        {
            // We can finally open the main log's file; this will flush out any data it already buffered.
            char logFilePath[512];
            Snprintf(logFilePath, sizeof(logFilePath), "%s/pal_calls.json", LogDirPath());

            result = m_pMainLog->OpenFile(logFilePath);
        }

        // If multithreaded logging is enabled, we need to go back over our previously allocated ThreadData and give
        // them a context.
        if ((result == Result::Success) && settings.interfaceLoggerConfig.multithreaded)
        {
            m_flags.multithreaded = 1;

            for (uint32 idx = 0; idx < m_threadDataVec.NumElements(); ++idx)
            {
                auto*const pThreadData = m_threadDataVec.At(idx);
                PAL_ASSERT(pThreadData->pContext == nullptr);

                pThreadData->pContext = CreateThreadLogContext(pThreadData->threadId);

                if (pThreadData->pContext == nullptr)
                {
                    // We failed to allocate a context, return an error and fall back to single-threaded logging.
                    result = Result::ErrorOutOfMemory;
                    m_flags.multithreaded = 0;
                    break;
                }
            }
        }

        // If no errors have occured then the log directory is ready for logging.
        m_flags.settingsCommitted = (result == Result::Success);

        // This assert will probably trigger if our process doesn't have write access to the log directory.
        PAL_ASSERT(m_flags.settingsCommitted);
    }

    return result;
}

// =====================================================================================================================
void Platform::NotifyPresent()
{
    // Switch to elevated logging (preset index 1) if the user is currently holding Shift-F11.
    const uint32 nextPreset = IsKeyPressed(KeyCode::Shift_F11);
    const uint32 prevPreset = AtomicExchange(&m_activePreset, nextPreset);

    // If we've changed presets, we need to take the platform lock and write a notice to the main log file.
    if (prevPreset != nextPreset)
    {
        // Get the time now so that it's close to the AtomicExchange. If the time gap is too big, some multithreaded
        // log entries might seem to have been logged in the wrong preset.
        const uint64 time = GetTime();

        MutexAuto lock(&m_platformMutex);

        if (nextPreset == 1)
        {
            m_pMainLog->BeginMap(false);
            m_pMainLog->KeyAndValue("_type", "BeginElevatedLogging");
            m_pMainLog->KeyAndValue("time", time);
            m_pMainLog->EndMap();
        }
        else
        {
            PAL_ASSERT(nextPreset == 0);

            m_pMainLog->BeginMap(false);
            m_pMainLog->KeyAndValue("_type", "EndElevatedLogging");
            m_pMainLog->KeyAndValue("time", time);
            m_pMainLog->EndMap();
        }
    }
}

// =====================================================================================================================
// Returns the current clock time in ticks relative to the starting time.
uint64 Platform::GetTime() const
{
    uint64 ticks = 0;

#if   defined(__unix__)
    RawTimerVal time;
    const int result = clock_gettime(CLOCK_MONOTONIC, &time);
    PAL_ASSERT(result == 0);

    // The number of nanoseconds (and ticks) in a second.
    constexpr long OneBillion = 1000 * 1000 * 1000;

    const bool   borrow = time.tv_nsec < m_startTime.tv_nsec;
    const long   nsec   = time.tv_nsec - m_startTime.tv_nsec + borrow * OneBillion;
    const time_t sec    = time.tv_sec  - m_startTime.tv_sec  - borrow;

    ticks = static_cast<uint64>(sec * OneBillion + nsec);
#endif

    return ticks;
}

// =====================================================================================================================
bool Platform::LogBeginFunc(
    const BeginFuncInfo& info,
    LogContext**         ppContext)
{
    // Log this function if the current preset contains one of the bits from its entry in the logging table.
    const uint32 funcIdx = static_cast<uint32>(info.funcId);
    bool         canLog  = (m_loggingPresets[m_activePreset] & FuncLoggingTable[funcIdx].logFlagMask) != 0;

    if (canLog)
    {
        ThreadData* pThreadData = static_cast<ThreadData*>(GetThreadLocalValue(m_threadKey));

        if (pThreadData == nullptr)
        {
            // This thread doesn't have a ThreadData yet, create a new one.
            MutexAuto lock(&m_platformMutex);
            pThreadData = CreateThreadData();
        }

        if (pThreadData == nullptr)
        {
            // Something went wrong when allocating the ThreadData. The only way to recover is to skip logging.
            PAL_ASSERT_ALWAYS();
            canLog = false;
        }
        else
        {
            if (m_flags.multithreaded == 1)
            {
                *ppContext = pThreadData->pContext;
            }
            else
            {
                *ppContext = m_pMainLog;

                // In single-threaded mode, we hold the platform mutex while logging each function.
                m_platformMutex.Lock();
            }

            (*ppContext)->BeginFunc(info, pThreadData->threadId);
        }
    }

    return canLog;
}

// =====================================================================================================================
void Platform::LogEndFunc(
    LogContext* pContext)
{
    pContext->EndFunc();

    if (m_flags.multithreaded == 0)
    {
        // In single-threaded mode, we hold the platform mutex while logging each function.
        m_platformMutex.Unlock();
    }
}

// =====================================================================================================================
Result Platform::EnumerateDevices(
    uint32*  pDeviceCount,
    IDevice* pDevices[MaxDevices])
{
    Result result = Result::Success;

    if (m_layerEnabled)
    {
        // We must tear down our GPUs before calling EnumerateDevices() because TearDownGpus() will call Cleanup()
        // which will destroy any state set by the lower layers in EnumerateDevices().
        TearDownGpus();

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::PlatformEnumerateDevices;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = GetTime();
        result                = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);
        funcInfo.postCallTime = GetTime();

        if (result == Result::Success)
        {
            m_deviceCount = (*pDeviceCount);
            for (uint32 i = 0; i < m_deviceCount; i++)
            {
                const uint32 deviceId = NewObjectId(InterfaceObject::Device);

                m_pDevices[i] = PAL_NEW(Device, this, SystemAllocType::AllocObject)(this, pDevices[i], deviceId);
                pDevices[i]->SetClientData(m_pDevices[i]);
                pDevices[i]   = m_pDevices[i];

                if (m_pDevices[i] == nullptr)
                {
                    result = Result::ErrorOutOfMemory;
                    break;
                }
            }
        }

        LogContext* pLogContext = nullptr;
        if (LogBeginFunc(funcInfo, &pLogContext))
        {
            pLogContext->BeginOutput();
            pLogContext->KeyAndEnum("result", result);
            pLogContext->KeyAndBeginList("devices", false);

            for (uint32 idx = 0; idx < m_deviceCount; ++idx)
            {
                pLogContext->Object(m_pDevices[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndOutput();

            LogEndFunc(pLogContext);
        }
    }
    else
    {
        result = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);
    }

    return result;
}

// =====================================================================================================================
size_t Platform::GetScreenObjectSize() const
{
    size_t screenSize = m_pNextLayer->GetScreenObjectSize();

    // We only want to wrap the screen with a decorator when the layer is enabled.  Otherwise, just pass the call
    // through.  This is a consequence of the fact that the Platform object is always wrapped, regardless of whether
    // the layer is actually enabled or not.
    if (m_layerEnabled)
    {
        screenSize += sizeof(Screen);
    }

    return screenSize;
}

// =====================================================================================================================
Result Platform::GetScreens(
    uint32*  pScreenCount,
    void*    pStorage[MaxScreens],
    IScreen* pScreens[MaxScreens])
{
    Result result = Result::Success;

    // We only want to wrap the screen with a decorator when the layer is enabled.  Otherwise, just pass the call
    // through.  This is a consequence of the fact that the Platform object is always wrapped, regardless of whether
    // the layer is actually enabled or not.
    if (m_layerEnabled)
    {
        PAL_ASSERT((pScreenCount != nullptr) && (pStorage != nullptr) && (pScreens != nullptr));

        IScreen* pNextScreens[MaxScreens] = {};
        void*    pNextStorage[MaxScreens] = {};

        for (uint32 i = 0; i < MaxScreens; i++)
        {
            PAL_ASSERT(pStorage[i] != nullptr);

            pNextStorage[i] = NextObjectAddr<Screen>(pStorage[i]);
        }

        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::PlatformGetScreens;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = GetTime();
        result                = m_pNextLayer->GetScreens(pScreenCount, pNextStorage, pNextScreens);
        funcInfo.postCallTime = GetTime();

        if (result == Result::Success)
        {
            const uint32 outScreenCount = *pScreenCount;
            for (uint32 screen = 0; screen < outScreenCount; screen++)
            {
                PAL_ASSERT(pNextScreens[screen] != nullptr);
                pNextScreens[screen]->SetClientData(pStorage[screen]);

                pScreens[screen] = PAL_PLACEMENT_NEW(pStorage[screen]) Screen(pNextScreens[screen],
                                                                              &m_pDevices[0],
                                                                              m_deviceCount,
                                                                              NewObjectId(InterfaceObject::Screen));
            }
        }

        LogContext* pLogContext = nullptr;
        if (LogBeginFunc(funcInfo, &pLogContext))
        {
            pLogContext->BeginOutput();
            pLogContext->KeyAndEnum("result", result);
            pLogContext->KeyAndBeginList("screens", false);

            for (uint32 idx = 0; idx < *pScreenCount; ++idx)
            {
                pLogContext->Object(pScreens[idx]);
            }

            pLogContext->EndList();
            pLogContext->EndOutput();

            LogEndFunc(pLogContext);
        }
    }
    else
    {
        result = m_pNextLayer->GetScreens(pScreenCount, pStorage, pScreens);
    }

    return result;
}

// =====================================================================================================================
void Platform::Destroy()
{
    if (m_layerEnabled)
    {
        // Note that we can't time a Destroy call.
        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::PlatformDestroy;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = GetTime();
        funcInfo.postCallTime = funcInfo.preCallTime;

        LogContext* pLogContext = nullptr;
        if (LogBeginFunc(funcInfo, &pLogContext))
        {
            LogEndFunc(pLogContext);
        }
    }

    PlatformDecorator::Destroy();
}

// =====================================================================================================================
void PAL_STDCALL Platform::InterfaceLoggerCb(
    void*                   pPrivateData,
    const uint32            deviceIndex,
    Developer::CallbackType type,
    void*                   pCbData)
{
    PAL_ASSERT(pPrivateData != nullptr);
    Platform* pPlatform = static_cast<Platform*>(pPrivateData);

    switch (type)
    {
    case Developer::CallbackType::AllocGpuMemory: // fallthrough intentional
    case Developer::CallbackType::FreeGpuMemory:
    case Developer::CallbackType::SubAllocGpuMemory:
    case Developer::CallbackType::SubFreeGpuMemory:
        PAL_ASSERT(pCbData != nullptr);
        TranslateGpuMemoryData(pCbData);
        break;
    case Developer::CallbackType::PresentConcluded:
    case Developer::CallbackType::CreateImage:
    case Developer::CallbackType::SurfRegData:
        break;
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
    case Developer::CallbackType::ImageBarrier:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBarrierEventData(pCbData);
        break;
    case Developer::CallbackType::DrawDispatch:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchData(pCbData);
        break;
    case Developer::CallbackType::BindPipeline:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindPipelineData(pCbData);
        break;
#if PAL_DEVELOPER_BUILD
    case Developer::CallbackType::DrawDispatchValidation:
        PAL_ASSERT(pCbData != nullptr);
        TranslateDrawDispatchValidationData(pCbData);
        break;
    case Developer::CallbackType::OptimizedRegisters:
        PAL_ASSERT(pCbData != nullptr);
        TranslateOptimizedRegistersData(pCbData);
        break;
#endif
    case Developer::CallbackType::BindGpuMemory:
        PAL_ASSERT(pCbData != nullptr);
        TranslateBindGpuMemoryData(pCbData);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    pPlatform->DeveloperCb(deviceIndex, type, pCbData);
}

// =====================================================================================================================
// Creates a new ThreadData for the current thread. The platform mutex must be locked when this is called.
Platform::ThreadData* Platform::CreateThreadData()
{
    ThreadData* pThreadData = PAL_NEW(ThreadData, this, AllocInternal);

    if (pThreadData != nullptr)
    {
        pThreadData->threadId = m_nextThreadId++;
        pThreadData->pContext = nullptr;

        Result result = Result::Success;

        // Create a log context for this thread if multithreaded logging is enabled.
        if (m_flags.multithreaded == 1)
        {
            pThreadData->pContext = CreateThreadLogContext(pThreadData->threadId);

            if (pThreadData->pContext == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
        }

        if (result == Result::Success)
        {
            // Add the context to our vector so we can delete it later.
            result = m_threadDataVec.PushBack(pThreadData);
        }

        if (result == Result::Success)
        {
            // Update the thread-local store so we can reuse this context.
            result = SetThreadLocalValue(m_threadKey, pThreadData);

            if (result != Result::Success)
            {
                // We successfully pushed our ThreadData into the vector but couldn't update the TLS. We should remove
                // it from the vector before we delete it.
                m_threadDataVec.PopBack(nullptr);
            }
        }

        if (result != Result::Success)
        {
            PAL_SAFE_DELETE(pThreadData->pContext, this);
            PAL_SAFE_DELETE(pThreadData, this);
        }
    }

    return pThreadData;
}

// =====================================================================================================================
// Creates a new LogContext for multi-threaded logging. The platform mutex must be locked when this is called.
LogContext* Platform::CreateThreadLogContext(
    uint32 threadId)
{
    LogContext* pContext = PAL_NEW(LogContext, this, AllocInternal)(this);

    if (pContext != nullptr)
    {
        // Create a file name and path for this log.
        char logFileName[64];
        Snprintf(logFileName, sizeof(logFileName), "pal_calls_thread_%u.json", threadId);

        char logFilePath[512];
        Snprintf(logFilePath, sizeof(logFilePath), "%s/%s", LogDirPath(), logFileName);

        const Result result = pContext->OpenFile(logFilePath);

        if (result == Result::Success)
        {
            // Add an entry to the main log that gives the name of this new log.
            m_pMainLog->BeginMap(false);
            m_pMainLog->KeyAndValue("_type", "LogFile");
            m_pMainLog->KeyAndValue("name", logFileName);
            m_pMainLog->EndMap();
        }
        else
        {
            PAL_SAFE_DELETE(pContext, this);
        }
    }

    return pContext;
}

// =====================================================================================================================
// Send turboSync control
Result Platform::TurboSyncControl(
    const TurboSyncControlInput& turboSyncControlInput)
{
    Result result = Result::Success;

    if (m_layerEnabled)
    {
        BeginFuncInfo funcInfo;
        funcInfo.funcId       = InterfaceFunc::PlatformTurboSyncControl;
        funcInfo.objectId     = m_objectId;
        funcInfo.preCallTime  = GetTime();
        result                = PlatformDecorator::TurboSyncControl(turboSyncControlInput);
        funcInfo.postCallTime = GetTime();

        LogContext* pLogContext = nullptr;
        if (LogBeginFunc(funcInfo, &pLogContext))
        {
            pLogContext->BeginInput();
            pLogContext->KeyAndStruct("turboSyncControlInput", turboSyncControlInput);
            pLogContext->EndInput();

            pLogContext->BeginOutput();
            pLogContext->KeyAndEnum("result", result);
            pLogContext->EndOutput();

            LogEndFunc(pLogContext);
        }
    }
    else
    {
        result = m_pNextLayer->TurboSyncControl(turboSyncControlInput);
    }

    return result;
}

} // InterfaceLogger
} // Pal

#endif
