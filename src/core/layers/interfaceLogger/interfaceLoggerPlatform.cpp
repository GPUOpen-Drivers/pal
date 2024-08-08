/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

static constexpr uint32 GenCalls     = LogFlagGeneralCalls;
static constexpr uint32 CrtDstry     = LogFlagCreateDestroy;
static constexpr uint32 BindMem      = LogFlagBindGpuMemory;
static constexpr uint32 QueueOps     = LogFlagQueueOps;
static constexpr uint32 CmdBuild     = LogFlagCmdBuilding;
static constexpr uint32 CrtSrds      = LogFlagCreateSrds;
static constexpr uint32 Callbacks    = LogFlagCallbacks;
static constexpr uint32 BarrierLog   = LogFlagBarrierLog;   // Barrier log cmd build calls in frame range control
static constexpr uint32 BarrierLogCr = LogFlagBarrierLogCr; // Internal only flag. Barrier log image and cmd buffer
                                                            // create calls that are not in frame range control.
                                                            // Unconditionally logged in both elevated and non-elevated
                                                            // modes when LogFlagBarrierLog is enabled.

struct FuncLoggingTableEntry
{
    InterfaceFunc function;    // The interface function this entry represents.
    uint32        logFlagMask; // The mask of all LogFlag bits that apply to this function.
};

static constexpr FuncLoggingTableEntry FuncLoggingTable[] =
{
    { InterfaceFunc::BorderColorPaletteUpdate,                      (GenCalls)                         },
    { InterfaceFunc::BorderColorPaletteBindGpuMemory,               (BindMem)                          },
    { InterfaceFunc::BorderColorPaletteDestroy,                     (CrtDstry | BindMem)               },
    { InterfaceFunc::CmdAllocatorReset,                             (GenCalls | CmdBuild)              },
    { InterfaceFunc::CmdAllocatorTrim,                              (GenCalls | CmdBuild)              },
    { InterfaceFunc::CmdAllocatorDestroy,                           (CrtDstry | CmdBuild)              },
    { InterfaceFunc::CmdBufferBegin,                                (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferEnd,                                  (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferReset,                                (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindPipeline,                      (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdPrimeGpuCaches,                    (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindMsaaState,                     (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSaveGraphicsState,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdRestoreGraphicsState,              (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindColorBlendState,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindDepthStencilState,             (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetDepthBounds,                    (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetUserData,                       (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdDuplicateUserData,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetKernelArguments,                (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetVertexBuffers,                  (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindIndexData,                     (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindTargets,                       (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindStreamOutTargets,              (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetPerDrawVrsRate,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetVrsCenterState,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindSampleRateImage,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdResolvePrtPlusImage,               (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdSetBlendConst,                     (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetInputAssemblyState,             (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetTriangleRasterState,            (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetPointLineRasterState,           (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetLineStippleState,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetDepthBiasState,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetStencilRefMasks,                (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetUserClipPlanes,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetMsaaQuadSamplePattern,          (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetViewports,                      (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetScissorRects,                   (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetGlobalScissor,                  (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBarrier,                           (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdRelease,                           (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdAcquire,                           (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdReleaseEvent,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdAcquireEvent,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdReleaseThenAcquire,                (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDraw,                              (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDrawOpaque,                        (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDrawIndexed,                       (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDrawIndirectMulti,                 (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDrawIndexedIndirectMulti,          (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDispatch,                          (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDispatchIndirect,                  (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDispatchOffset,                    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDispatchMesh,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdDispatchMeshIndirectMulti,         (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyMemory,                        (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryByGpuVa,                 (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyImage,                         (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToImage,                 (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyImageToMemory,                 (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToTiledImage,            (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyTiledImageToMemory,            (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyTypedBuffer,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdScaledCopyTypedBufferToImage,      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCopyRegisterToMemory,              (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdScaledCopyImage,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdGenerateMipmaps,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdColorSpaceConversionCopy,          (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCloneImageData,                    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdUpdateMemory,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdUpdateBusAddressableMemoryMarker,  (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdFillMemory,                        (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearColorBuffer,                  (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearBoundColorTargets,            (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearColorImage,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearBoundDepthStencilTargets,     (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearDepthStencil,                 (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearBufferView,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdClearImageView,                    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdResolveImage,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdSetEvent,                          (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdResetEvent,                        (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdPredicateEvent,                    (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdMemoryAtomic,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdBeginQuery,                        (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdEndQuery,                          (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdResolveQuery,                      (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdResetQueryPool,                    (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdWriteTimestamp,                    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdWriteImmediate,                    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdLoadBufferFilledSizes,             (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSaveBufferFilledSizes,             (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetBufferFilledSize,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdBindBorderColorPalette,            (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetPredication,                    (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSuspendPredication,                (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdIf,                                (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdElse,                              (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdEndIf,                             (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdWhile,                             (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdEndWhile,                          (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdWaitRegisterValue,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdWaitMemoryValue,                   (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdWaitBusAddressableMemoryMarker,    (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdLoadCeRam,                         (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdDumpCeRam,                         (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdWriteCeRam,                        (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdAllocateEmbeddedData,              (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdAllocateLargeEmbeddedData,         (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdExecuteNestedCmdBuffers,           (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdSaveComputeState,                  (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdRestoreComputeState,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdExecuteIndirectCmds,               (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdSetMarker,                         (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdPresent,                           (CmdBuild | BarrierLog)            },
    { InterfaceFunc::CmdBufferCmdCommentString,                     (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdNop,                               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdStartGpuProfilerLogging,           (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdStopGpuProfilerLogging,            (CmdBuild)                         },
    { InterfaceFunc::CmdBufferDestroy,                              (CrtDstry | CmdBuild) },
    { InterfaceFunc::CmdBufferCmdSetViewInstanceMask,               (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdUpdateHiSPretests,                 (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdSetClipRects,                      (CmdBuild)                         },
    { InterfaceFunc::CmdBufferCmdPostProcessFrame,                  (CmdBuild | BarrierLog)            },
    { InterfaceFunc::ColorBlendStateDestroy,                        (CrtDstry)                         },
    { InterfaceFunc::DepthStencilStateDestroy,                      (CrtDstry)                         },
    { InterfaceFunc::DeviceCommitSettingsAndInit,                   (GenCalls)                         },
    { InterfaceFunc::DeviceFinalize,                                (GenCalls)                         },
    { InterfaceFunc::DeviceCleanup,                                 (GenCalls)                         },
    { InterfaceFunc::DeviceSetMaxQueuedFrames,                      (GenCalls | QueueOps)              },
    { InterfaceFunc::DeviceAddGpuMemoryReferences,                  (GenCalls)                         },
    { InterfaceFunc::DeviceRemoveGpuMemoryReferences,               (GenCalls)                         },
    { InterfaceFunc::DeviceSetClockMode,                            (GenCalls)                         },
    { InterfaceFunc::DeviceSetMgpuMode,                             (GenCalls)                         },
    { InterfaceFunc::DeviceOfferAllocations,                        (GenCalls)                         },
    { InterfaceFunc::DeviceReclaimAllocations,                      (GenCalls)                         },
    { InterfaceFunc::DeviceResetFences,                             (GenCalls)                         },
    { InterfaceFunc::DeviceWaitForFences,                           (GenCalls)                         },
    { InterfaceFunc::DeviceBindTrapHandler,                         (GenCalls)                         },
    { InterfaceFunc::DeviceBindTrapBuffer,                          (GenCalls)                         },
    { InterfaceFunc::DeviceCreateQueue,                             (CrtDstry | QueueOps)              },
    { InterfaceFunc::DeviceCreateMultiQueue,                        (CrtDstry | QueueOps)              },
    { InterfaceFunc::DeviceCreateGpuMemory,                         (CrtDstry)                         },
    { InterfaceFunc::DeviceCreatePinnedGpuMemory,                   (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateSvmGpuMemory,                      (CrtDstry)                         },
    { InterfaceFunc::DeviceOpenSharedGpuMemory,                     (CrtDstry)                         },
    { InterfaceFunc::DeviceOpenExternalSharedGpuMemory,             (CrtDstry)                         },
    { InterfaceFunc::DeviceOpenPeerGpuMemory,                       (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateImage,                             (CrtDstry | BarrierLogCr)          },
    { InterfaceFunc::DeviceCreatePresentableImage,                  (CrtDstry | BarrierLogCr)          },
    { InterfaceFunc::DeviceOpenPeerImage,                           (CrtDstry | BarrierLogCr)          },
    { InterfaceFunc::DeviceOpenExternalSharedImage,                 (CrtDstry | BarrierLogCr)          },
    { InterfaceFunc::DeviceCreateColorTargetView,                   (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateDepthStencilView,                  (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateTypedBufferViewSrds,               (CrtSrds)                          },
    { InterfaceFunc::DeviceCreateUntypedBufferViewSrds,             (CrtSrds)                          },
    { InterfaceFunc::DeviceCreateImageViewSrds,                     (CrtSrds)                          },
    { InterfaceFunc::DeviceCreateFmaskViewSrds,                     (CrtSrds)                          },
    { InterfaceFunc::DeviceCreateSamplerSrds,                       (CrtSrds)                          },
    { InterfaceFunc::DeviceCreateBvhSrds,                           (CrtSrds)                          },
    { InterfaceFunc::DeviceSetSamplePatternPalette,                 (GenCalls)                         },
    { InterfaceFunc::DeviceCreateBorderColorPalette,                (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateComputePipeline,                   (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateGraphicsPipeline,                  (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateShaderLibrary,                     (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateMsaaState,                         (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateColorBlendState,                   (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateDepthStencilState,                 (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateQueueSemaphore,                    (CrtDstry | QueueOps)              },
    { InterfaceFunc::DeviceOpenSharedQueueSemaphore,                (CrtDstry | QueueOps)              },
    { InterfaceFunc::DeviceOpenExternalSharedQueueSemaphore,        (CrtDstry | QueueOps)              },
    { InterfaceFunc::DeviceCreateFence,                             (CrtDstry)                         },
    { InterfaceFunc::DeviceOpenFence,                               (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateGpuEvent,                          (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateQueryPool,                         (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateCmdAllocator,                      (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateCmdBuffer,                         (CrtDstry | BarrierLogCr)          },
    { InterfaceFunc::DeviceCreateIndirectCmdGenerator,              (CrtDstry)                         },
    { InterfaceFunc::DeviceGetPrivateScreens,                       (CrtDstry)                         },
    { InterfaceFunc::DeviceAddEmulatedPrivateScreen,                (CrtDstry)                         },
    { InterfaceFunc::DeviceRemoveEmulatedPrivateScreen,             (CrtDstry)                         },
    { InterfaceFunc::DeviceCreatePrivateScreenImage,                (CrtDstry)                         },
    { InterfaceFunc::DeviceCreateSwapChain,                         (CrtDstry)                         },
    { InterfaceFunc::DeviceSetPowerProfile,                         (GenCalls)                         },
    { InterfaceFunc::DeviceFlglQueryState,                          (GenCalls)                         },
    { InterfaceFunc::DeviceFlglSetSyncConfiguration,                (GenCalls)                         },
    { InterfaceFunc::DeviceFlglGetSyncConfiguration,                (GenCalls)                         },
    { InterfaceFunc::DeviceFlglSetFrameLock,                        (GenCalls)                         },
    { InterfaceFunc::DeviceFlglSetGenLock,                          (GenCalls)                         },
    { InterfaceFunc::DeviceFlglResetFrameCounter,                   (GenCalls)                         },
    { InterfaceFunc::DeviceFlglGetFrameCounter,                     (GenCalls)                         },
    { InterfaceFunc::DeviceFlglGetFrameCounterResetStatus,          (GenCalls)                         },
    { InterfaceFunc::DeviceCreateVirtualDisplay,                    (CrtDstry)                         },
    { InterfaceFunc::DeviceDestroyVirtualDisplay,                   (CrtDstry)                         },
    { InterfaceFunc::DeviceGetVirtualDisplayProperties,             (GenCalls)                         },
    { InterfaceFunc::FenceDestroy,                                  (CrtDstry)                         },
    { InterfaceFunc::GpuEventSet,                                   (GenCalls)                         },
    { InterfaceFunc::GpuEventReset,                                 (GenCalls)                         },
    { InterfaceFunc::GpuEventBindGpuMemory,                         (BindMem)                          },
    { InterfaceFunc::GpuEventDestroy,                               (CrtDstry | BindMem)               },
    { InterfaceFunc::GpuMemorySetPriority,                          (GenCalls)                         },
    { InterfaceFunc::GpuMemoryMap,                                  (GenCalls)                         },
    { InterfaceFunc::GpuMemoryUnmap,                                (GenCalls)                         },
    { InterfaceFunc::GpuMemorySetSdiRemoteBusAddress,               (GenCalls)                         },
    { InterfaceFunc::GpuMemoryDestroy,                              (CrtDstry | BindMem)               },
    { InterfaceFunc::ImageBindGpuMemory,                            (BindMem)                          },
    { InterfaceFunc::ImageDestroy,                                  (CrtDstry | BindMem)               },
    { InterfaceFunc::IndirectCmdGeneratorBindGpuMemory,             (BindMem)                          },
    { InterfaceFunc::IndirectCmdGeneratorDestroy,                   (CrtDstry | BindMem)               },
    { InterfaceFunc::MsaaStateDestroy,                              (CrtDstry)                         },
    { InterfaceFunc::PipelineLinkWithLibraries,                     (GenCalls)                         },
    { InterfaceFunc::PipelineDestroy,                               (CrtDstry)                         },
    { InterfaceFunc::PlatformEnumerateDevices,                      (GenCalls)                         },
    { InterfaceFunc::PlatformGetScreens,                            (GenCalls)                         },
    { InterfaceFunc::PlatformTurboSyncControl,                      (GenCalls)                         },
    { InterfaceFunc::PlatformDestroy,                               (CrtDstry)                         },
    { InterfaceFunc::PrivateScreenEnable,                           (GenCalls)                         },
    { InterfaceFunc::PrivateScreenDisable,                          (GenCalls)                         },
    { InterfaceFunc::PrivateScreenBlank,                            (GenCalls)                         },
    { InterfaceFunc::PrivateScreenPresent,                          (GenCalls)                         },
    { InterfaceFunc::PrivateScreenSetGammaRamp,                     (GenCalls)                         },
    { InterfaceFunc::PrivateScreenSetPowerMode,                     (GenCalls)                         },
    { InterfaceFunc::PrivateScreenSetDisplayMode,                   (GenCalls)                         },
    { InterfaceFunc::PrivateScreenSetColorMatrix,                   (GenCalls)                         },
    { InterfaceFunc::PrivateScreenSetEventAfterVsync,               (GenCalls)                         },
    { InterfaceFunc::PrivateScreenEnableAudio,                      (GenCalls)                         },
    { InterfaceFunc::QueryPoolBindGpuMemory,                        (BindMem)                          },
    { InterfaceFunc::QueryPoolDestroy,                              (CrtDstry | BindMem)               },
    { InterfaceFunc::QueryPoolReset,                                (GenCalls)                         },
    { InterfaceFunc::QueueSubmit,                                   (QueueOps)                         },
    { InterfaceFunc::QueueWaitIdle,                                 (QueueOps)                         },
    { InterfaceFunc::QueueSignalQueueSemaphore,                     (QueueOps)                         },
    { InterfaceFunc::QueueWaitQueueSemaphore,                       (QueueOps)                         },
    { InterfaceFunc::QueuePresentDirect,                            (QueueOps)                         },
    { InterfaceFunc::QueuePresentSwapChain,                         (QueueOps)                         },
    { InterfaceFunc::QueueDelay,                                    (QueueOps)                         },
    { InterfaceFunc::QueueDelayAfterVsync,                          (QueueOps)                         },
    { InterfaceFunc::QueueRemapVirtualMemoryPages,                  (QueueOps)                         },
    { InterfaceFunc::QueueCopyVirtualMemoryPageMappings,            (QueueOps)                         },
    { InterfaceFunc::QueueAssociateFenceWithLastSubmit,             (QueueOps)                         },
    { InterfaceFunc::QueueSetExecutionPriority,                     (QueueOps)                         },
    { InterfaceFunc::QueueDestroy,                                  (CrtDstry | QueueOps)              },
    { InterfaceFunc::QueueSemaphoreDestroy,                         (CrtDstry | QueueOps)              },
    { InterfaceFunc::ScreenIsImplicitFullscreenOwnershipSafe,       (GenCalls)                         },
    { InterfaceFunc::ScreenQueryCurrentDisplayMode,                 (GenCalls)                         },
    { InterfaceFunc::ScreenTakeFullscreenOwnership,                 (GenCalls)                         },
    { InterfaceFunc::ScreenReleaseFullscreenOwnership,              (GenCalls)                         },
    { InterfaceFunc::ScreenSetGammaRamp,                            (GenCalls)                         },
    { InterfaceFunc::ScreenWaitForVerticalBlank,                    (GenCalls)                         },
    { InterfaceFunc::ScreenDestroy,                                 (CrtDstry)                         },
    { InterfaceFunc::ShaderLibraryDestroy,                          (CrtDstry)                         },
    { InterfaceFunc::SwapChainAcquireNextImage,                     (GenCalls | QueueOps)              },
    { InterfaceFunc::SwapChainWaitIdle,                             (GenCalls)                         },
    { InterfaceFunc::SwapChainDestroy,                              (CrtDstry)                         },
};

static_assert(ArrayLen(FuncLoggingTable) == size_t(InterfaceFunc::Count),
              "The FuncLoggingTable must be updated.");

// =====================================================================================================================
// Validates func logging table is setup correctly.
template <size_t N>
constexpr bool ValidateFuncLoggingTable(const FuncLoggingTableEntry (&table)[N])
{
    bool valid = true;
    for (uint32 i = 0; i < N; i++)
    {
        if (i != uint32(table[i].function))
        {
            valid = false;
            break;
        }
    }

    return valid;
}

static_assert(ValidateFuncLoggingTable(FuncLoggingTable), "Wrong funcId mapping in FuncLoggingTable!");

struct CallbackLoggingTableEntry
{
    Developer::CallbackType callbackType; // The callback function this entry represents.
    uint32                  logFlagMask;  // The mask of all LogFlag bits that apply to this function.
};

// Callbacks are only logged if they're triggered by an interface call which has logging enabled. In effect, the
// logFlagMask in FuncLoggingTable filters out callbacks before we even check the CallbackLoggingTable.
//
// Note the cases where logFlagMask = 0. These callbacks will never be logged no matter the preset values. Currently
// many callbacks are not useful for interface debugging so we filter them out here. If you have an interface logger
// use-case which would benefit from additonal callbacks feel free to add some log flags.
static constexpr CallbackLoggingTableEntry CallbackLoggingTable[] =
{
    { Developer::CallbackType::AllocGpuMemory,         (0)                      },
    { Developer::CallbackType::FreeGpuMemory,          (0)                      },
    { Developer::CallbackType::PresentConcluded,       (0)                      },
    { Developer::CallbackType::ImageBarrier,           (Callbacks | BarrierLog) },
    { Developer::CallbackType::CreateImage,            (0)                      },
    { Developer::CallbackType::BarrierBegin,           (Callbacks)              },
    { Developer::CallbackType::BarrierEnd,             (Callbacks | BarrierLog) },
    { Developer::CallbackType::DrawDispatch,           (0)                      },
    { Developer::CallbackType::BindPipeline,           (0)                      },
    { Developer::CallbackType::SurfRegData,            (0)                      },
    { Developer::CallbackType::DrawDispatchValidation, (0)                      },
    { Developer::CallbackType::BindPipelineValidation, (0)                      },
    { Developer::CallbackType::OptimizedRegisters,     (0)                      },
    { Developer::CallbackType::BindGpuMemory,          (0)                      },
    { Developer::CallbackType::SubAllocGpuMemory,      (0)                      },
    { Developer::CallbackType::SubFreeGpuMemory,       (0)                      },
    { Developer::CallbackType::RpmBlt,                 (Callbacks | BarrierLog) },
};

static_assert(ArrayLen(CallbackLoggingTable) == size_t(Developer::CallbackType::Count),
              "The CallbackLoggingTable must be updated.");

// =====================================================================================================================
// Validates callback logging table is setup correctly.
template <size_t N>
constexpr bool ValidateCallbackLoggingTable(const CallbackLoggingTableEntry (&table)[N])
{
    bool valid = true;
    for (uint32 i = 0; i < N; i++)
    {
        if (i != uint32(table[i].callbackType))
        {
            valid = false;
            break;
        }
    }

    return valid;
}

static_assert(ValidateCallbackLoggingTable(CallbackLoggingTable),
              "Wrong callbackType mapping in CallbackLoggingTable!");

// =====================================================================================================================
ThreadData::ThreadData(
    Platform* pPlatform,
    uint32    threadId)
    :
    m_pContext(nullptr),
    m_threadId(threadId),
    m_objectId(0),
    m_activeFunc(InterfaceFunc::Count),
    m_preCallTime(0),
    m_callbacks(pPlatform)
{
}

// =====================================================================================================================
ThreadData::~ThreadData()
{
    Platform*const pPlatform = m_callbacks.GetAllocator();

    PAL_SAFE_DELETE(m_pContext, pPlatform);
}

// =====================================================================================================================
void ThreadData::SetContext(
    LogContext* pContext)
{
    // The Platform creates log contexts some time after it constructs ThreadData objects so it needs a setter to
    // give us our context. It must never call this function more than once, this assert makes sure of that.
    PAL_ASSERT(m_pContext == nullptr);

    m_pContext = pContext;
}

// =====================================================================================================================
void ThreadData::StartCall(
    uint32        objectId,
    InterfaceFunc func,
    uint64        preCallTime)
{
    m_objectId    = objectId;
    m_activeFunc  = func;
    m_preCallTime = preCallTime;
}

// =====================================================================================================================
void ThreadData::EndCall()
{
    // Set this to "Count" to indicate that this thread has finished calling its interface function.
    m_activeFunc = InterfaceFunc::Count;
}

// =====================================================================================================================
void ThreadData::PushBackCallbackArgs(
    Developer::CallbackType callbackType,
    const void*             pCallbackData,
    size_t                  dataSize)
{
    // Note that the OptimizedRegisters callback requires a deep copy of a few arrays which a simple memcpy can't do.
    // Currently we don't want to log this callback but if that ever changes we need a ThreadData refactor to handle
    // the deep copy. If someone adds OptimizedRegisters logging to InterfaceLoggerCb this will catch it.
    PAL_ASSERT(callbackType != Developer::CallbackType::OptimizedRegisters);

    DevCallbackArgs args = { .callbackType = callbackType };

    if (dataSize > 0)
    {
        // If this fails someone needs to update the union in DevCallbackArgs.
        PAL_ASSERT(dataSize <= sizeof(args.data));
        memcpy(&args.data, pCallbackData, dataSize);
    }

    // This can fail but there's nothing we can do if it does. This function is called by InterfaceLoggerCb which can't
    // return a Result. Even if it could we wouldn't want PAL to fail the interface call just because we couldn't log
    // this callback. Perhaps it's best to do nothing here and try to let logging continue normally.
    const Result result = m_callbacks.PushBack(args);
    PAL_ASSERT(result == Result::Success);
}

// =====================================================================================================================
void ThreadData::ClearCallbackArgs()
{
    m_callbacks.Clear();
}

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
    m_threadDataVec(this),
    m_frameCount(0)
{
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

    for (ThreadData*const pThreadData : m_threadDataVec)
    {
        PAL_DELETE(pThreadData, this);
    }

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

        // When barrier log mode is enabled, OR internal flag LogFlagBarrierLogCr to log image and cmd buffer create
        // calls unconditionally in both base and elevated modes.
        if (TestAnyFlagSet(m_loggingPresets[0] | m_loggingPresets[1], LogFlagBarrierLog))
        {
            m_loggingPresets[0] |= LogFlagBarrierLogCr;
            m_loggingPresets[1] |= LogFlagBarrierLogCr;
        }

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

            for (ThreadData*const pThreadData : m_threadDataVec)
            {
                LogContext*const pContext = CreateThreadLogContext(pThreadData->ThreadId());

                if (pContext == nullptr)
                {
                    // We failed to allocate a context, return an error and fall back to single-threaded logging.
                    result = Result::ErrorOutOfMemory;
                    m_flags.multithreaded = 0;
                    break;
                }
                else
                {
                    pThreadData->SetContext(pContext);
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
void Platform::UpdatePresentState()
{
    Util::AtomicIncrement(&m_frameCount);

    // Switch to elevated logging (preset index 1) if the user is currently holding Shift-F11 or
    // inside targeted frame range (if there is range control) now.
    const uint32 nextPreset = IsKeyPressed(KeyCode::Shift_F11) || IsFrameRangeActive();
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

        // Flush this directly to the main log file. That way we'll see this data even if the app crashes
        // or exits without destroying our platform.
        m_pMainLog->Flush();
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
bool Platform::ActivateLogging(
    uint32        objectId,
    InterfaceFunc func)
{
    // Log this function if the current preset contains one of the bits from its entry in the logging table.
    bool canLog = (m_loggingPresets[m_activePreset] & FuncLoggingTable[uint32(func)].logFlagMask) != 0;

    if (canLog)
    {
        auto* pThreadData = static_cast<ThreadData*>(GetThreadLocalValue(m_threadKey));

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
            // Store this state in our thread local data so we can:
            // 1. Detect if we're actively logging a function when we get a developer callback.
            // 2. Automatically log this data in LogEndFunc.
            pThreadData->StartCall(objectId, func, GetTime());
        }
    }

    return canLog;
}

// =====================================================================================================================
// Return true if there is frame range control and current frame is in the range; return false otherwise.
bool Platform::IsFrameRangeActive() const
{
    const auto&  config     = PlatformSettings().interfaceLoggerConfig;
    const uint32 frameStart = config.startFrame;
    const uint32 frameEnd   = frameStart + config.frameCount - 1;
    const uint32 curFrame   = FrameCount();

    return (config.frameCount != 0) && ((curFrame >= frameStart) && (curFrame <= frameEnd));
}

// =====================================================================================================================
LogContext* Platform::LogBeginFunc()
{
    // Call GetTime first so that it's as close as possible to when the caller called the next layer.
    const uint64 postCallTime = GetTime();
    LogContext*  pContext     = nullptr;
    auto*const   pThreadData  = static_cast<ThreadData*>(GetThreadLocalValue(m_threadKey));

    if (pThreadData == nullptr)
    {
        // It should be impossible to get here if the caller respected the return value of ActivateLogging!
        PAL_ASSERT_ALWAYS();
    }
    else
    {
        // In multithreaded mode each ThreadData allocates its own independent log context. Otherwise we need to use
        // the single shared m_pMainLog which is owned by the platform. Note that pThreadData->Context() cannot return
        // m_pMainLog because that pointer communicates pointer ownership. We'd risk a double-free if we set the
        // ThreadData pointer to m_pMainLog.
        if (m_flags.multithreaded == 1)
        {
            pContext = pThreadData->Context();
        }
        else
        {
            pContext = m_pMainLog;

            // In single-threaded mode, we hold the platform mutex while logging each function.
            m_platformMutex.Lock();
        }

        pContext->BeginFunc(pThreadData->ObjectId(),
                            pThreadData->ActiveFunc(),
                            pThreadData->ThreadId(),
                            pThreadData->PreCallTime(),
                            postCallTime);

        // This must be last in this function.
        pThreadData->EndCall();
    }

    return pContext;
}

// =====================================================================================================================
void Platform::LogEndFunc(
    LogContext* pContext)
{
    auto*const pThreadData = static_cast<ThreadData*>(GetThreadLocalValue(m_threadKey));

    // Only add the "callbacks" key if a callback was actually called.
    if (pThreadData->HasCallbacks())
    {
        pContext->KeyAndBeginList("callbacks", false);

        for (const DevCallbackArgs& args : pThreadData->Callbacks())
        {
            pContext->Struct(args);
        }

        pContext->EndList();

        // Always clear the vector so that we don't log these again on the next function call.
        pThreadData->ClearCallbackArgs();
    }

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

        const bool active = ActivateLogging(m_objectId, InterfaceFunc::PlatformEnumerateDevices);

        result = m_pNextLayer->EnumerateDevices(pDeviceCount, pDevices);

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

        if (active)
        {
            LogContext*const pLogContext = LogBeginFunc();

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

        const bool active = ActivateLogging(m_objectId, InterfaceFunc::PlatformGetScreens);

        result = m_pNextLayer->GetScreens(pScreenCount, pNextStorage, pNextScreens);

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

        if (active)
        {
            LogContext*const pLogContext = LogBeginFunc();

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
        // Note that we can't time Destroy calls nor track their callbacks.
        if (ActivateLogging(m_objectId, InterfaceFunc::PlatformDestroy))
        {
            LogContext*const pLogContext = LogBeginFunc();

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
    Platform*const pThis = static_cast<Platform*>(pPrivateData);

    size_t dataSize = 0;

    switch (type)
    {
    case Developer::CallbackType::AllocGpuMemory:
    case Developer::CallbackType::FreeGpuMemory:
    case Developer::CallbackType::SubAllocGpuMemory:
    case Developer::CallbackType::SubFreeGpuMemory:
        TranslateGpuMemoryData(pCbData);
        dataSize = sizeof(Developer::GpuMemoryData);
        break;
    case Developer::CallbackType::PresentConcluded:
        dataSize = sizeof(Developer::PresentationModeData);
        break;
    case Developer::CallbackType::CreateImage:
        dataSize = sizeof(Developer::ImageDataAddrMgrSurfInfo);
        break;
    case Developer::CallbackType::SurfRegData:
        dataSize = sizeof(Developer::SurfRegDataInfo);
        break;
    case Developer::CallbackType::BarrierBegin:
    case Developer::CallbackType::BarrierEnd:
    case Developer::CallbackType::ImageBarrier:
        TranslateBarrierEventData(pCbData);
        dataSize = sizeof(Developer::BarrierData);
        break;
    case Developer::CallbackType::DrawDispatch:
        TranslateDrawDispatchData(pCbData);
        dataSize = sizeof(Developer::DrawDispatchData);
        break;
    case Developer::CallbackType::BindPipeline:
        TranslateBindPipelineData(pCbData);
        dataSize = sizeof(Developer::BindPipelineData);
        break;
    case Developer::CallbackType::DrawDispatchValidation:
        TranslateDrawDispatchValidationData(pCbData);
        dataSize = sizeof(Developer::DrawDispatchValidationData);
        break;
    case Developer::CallbackType::BindPipelineValidation:
        TranslateBindPipelineValidationData(pCbData);
        dataSize = sizeof(Developer::BindPipelineValidationData);
        break;
    case Developer::CallbackType::OptimizedRegisters:
        TranslateOptimizedRegistersData(pCbData);
        dataSize = sizeof(Developer::OptimizedRegistersData);
        break;
    case Developer::CallbackType::BindGpuMemory:
        TranslateBindGpuMemoryData(pCbData);
        dataSize = sizeof(Developer::BindGpuMemoryData);
        break;
    case Developer::CallbackType::RpmBlt:
        TranslateReportRpmBltTypeData(pCbData);
        dataSize = sizeof(Developer::RpmBltData);
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    // Log this callback if the current preset contains one of the bits from its entry in the logging table.
    if (TestAnyFlagSet(pThis->m_loggingPresets[pThis->m_activePreset], CallbackLoggingTable[uint32(type)].logFlagMask))
    {
        auto*const pThreadData = static_cast<ThreadData*>(GetThreadLocalValue(pThis->m_threadKey));

        // This if-statement filters out two kinds of callbacks:
        // 1. Callbacks on PAL-internal threads. These are rare but they do happen! We chose to ignore them.
        // 2. Callbacks during interface calls that aren't decorated or that have logging disabled. We want to ignore
        //    these too because we need a full ActivateLogging/LogBeginFunc/LogEndFunc sequence to log callbacks.
        //    Note that this means the interface function presets implicitly filter callbacks.
        if ((pThreadData != nullptr) && pThreadData->LoggingActive())
        {
            pThreadData->PushBackCallbackArgs(type, pCbData, dataSize);
        }
    }

    pThis->DeveloperCb(deviceIndex, type, pCbData);
}

// =====================================================================================================================
// Creates a new ThreadData for the current thread. The platform mutex must be locked when this is called.
ThreadData* Platform::CreateThreadData()
{
    ThreadData* pThreadData = PAL_NEW(ThreadData, this, AllocInternal)(this, m_nextThreadId++);

    if (pThreadData != nullptr)
    {
        Result result = Result::Success;

        // Create a log context for this thread if multithreaded logging is enabled. Note that we should never call
        // pThreadData->SetContext(m_pMainLog) because SetContext transfers pointer ownership. The ThreadData will
        // free its log context when it's deleted.
        if (m_flags.multithreaded == 1)
        {
            LogContext*const pContext = CreateThreadLogContext(pThreadData->ThreadId());

            if (pContext == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                pThreadData->SetContext(pContext);
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
            // Note that ThreadData will automatically free the LogContext we created for it.
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

            // Flush this directly to the main log file. That way we'll see this data even if the app crashes
            // or exits without destroying our platform.
            m_pMainLog->Flush();
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
        const bool active = ActivateLogging(m_objectId, InterfaceFunc::PlatformTurboSyncControl);

        result = PlatformDecorator::TurboSyncControl(turboSyncControlInput);

        if (active)
        {
            LogContext*const pLogContext = LogBeginFunc();

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

// =====================================================================================================================
bool Platform::IsBarrierLogActive() const
{
    // Determine if this is for barrier log only mode.
    return TestAllFlagsSet(LogFlagBarrierLogCr | LogFlagBarrierLog, m_loggingPresets[m_activePreset]);
}

} // InterfaceLogger
} // Pal

#endif
