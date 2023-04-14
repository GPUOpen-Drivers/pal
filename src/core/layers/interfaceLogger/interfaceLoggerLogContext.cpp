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

#include "core/layers/interfaceLogger/interfaceLoggerBorderColorPalette.h"
#include "core/layers/interfaceLogger/interfaceLoggerCmdAllocator.h"
#include "core/layers/interfaceLogger/interfaceLoggerCmdBuffer.h"
#include "core/layers/interfaceLogger/interfaceLoggerColorBlendState.h"
#include "core/layers/interfaceLogger/interfaceLoggerColorTargetView.h"
#include "core/layers/interfaceLogger/interfaceLoggerDepthStencilState.h"
#include "core/layers/interfaceLogger/interfaceLoggerDepthStencilView.h"
#include "core/layers/interfaceLogger/interfaceLoggerDevice.h"
#include "core/layers/interfaceLogger/interfaceLoggerFence.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuEvent.h"
#include "core/layers/interfaceLogger/interfaceLoggerGpuMemory.h"
#include "core/layers/interfaceLogger/interfaceLoggerImage.h"
#include "core/layers/interfaceLogger/interfaceLoggerIndirectCmdGenerator.h"
#include "core/layers/interfaceLogger/interfaceLoggerLogContext.h"
#include "core/layers/interfaceLogger/interfaceLoggerMsaaState.h"
#include "core/layers/interfaceLogger/interfaceLoggerPipeline.h"
#include "core/layers/interfaceLogger/interfaceLoggerPlatform.h"
#include "core/layers/interfaceLogger/interfaceLoggerPrivateScreen.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueryPool.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueue.h"
#include "core/layers/interfaceLogger/interfaceLoggerQueueSemaphore.h"
#include "core/layers/interfaceLogger/interfaceLoggerScreen.h"
#include "core/layers/interfaceLogger/interfaceLoggerShaderLibrary.h"
#include "core/layers/interfaceLogger/interfaceLoggerSwapChain.h"

using namespace Util;

namespace Pal
{
namespace InterfaceLogger
{

const char*const ObjectNames[] =
{
    "IBorderColorPalette",
    "ICmdAllocator",
    "ICmdBuffer",
    "IColorBlendState",
    "IColorTargetView",
    "IDepthStencilState",
    "IDepthStencilView",
    "IDevice",
    "IFence",
    "IGpuEvent",
    "IGpuMemory",
    "IImage",
    "IIndirectCmdGenerator",
    "IMsaaState",
    "IPipeline",
    "IPlatform",
    "IPrivateScreen",
    "IQueryPool",
    "IQueue",
    "IQueueSemaphore",
    "IScreen",
    "IShaderLibrary",
    "ISwapChain",
};

static_assert(ArrayLen(ObjectNames) == static_cast<size_t>(InterfaceObject::Count),
              "The ObjectNames array must be updated.");

struct FuncFormattingEntry
{
    InterfaceFunc    function;   // The interface function this entry represents.
    InterfaceObject  objectType; // The object the function belongs to.
    const char*const pFuncName;  // The name of the function.
};

static constexpr FuncFormattingEntry FuncFormattingTable[] =
{
    { InterfaceFunc::BorderColorPaletteUpdate,                                  InterfaceObject::BorderColorPalette,   "Update"                                  },
    { InterfaceFunc::BorderColorPaletteBindGpuMemory,                           InterfaceObject::BorderColorPalette,   "BindGpuMemory"                           },
    { InterfaceFunc::BorderColorPaletteDestroy,                                 InterfaceObject::BorderColorPalette,   "Destroy"                                 },
    { InterfaceFunc::CmdAllocatorReset,                                         InterfaceObject::CmdAllocator,         "Reset"                                   },
    { InterfaceFunc::CmdAllocatorTrim,                                          InterfaceObject::CmdAllocator,         "Trim"                                    },
    { InterfaceFunc::CmdAllocatorDestroy,                                       InterfaceObject::CmdAllocator,         "Destroy"                                 },
    { InterfaceFunc::CmdBufferBegin,                                            InterfaceObject::CmdBuffer,            "Begin"                                   },
    { InterfaceFunc::CmdBufferEnd,                                              InterfaceObject::CmdBuffer,            "End"                                     },
    { InterfaceFunc::CmdBufferReset,                                            InterfaceObject::CmdBuffer,            "Reset"                                   },
    { InterfaceFunc::CmdBufferCmdBindPipeline,                                  InterfaceObject::CmdBuffer,            "CmdBindPipeline"                         },
    { InterfaceFunc::CmdBufferCmdPrimeGpuCaches,                                InterfaceObject::CmdBuffer,            "CmdPrimeGpuCaches"                       },
    { InterfaceFunc::CmdBufferCmdBindMsaaState,                                 InterfaceObject::CmdBuffer,            "CmdBindMsaaState"                        },
    { InterfaceFunc::CmdBufferCmdSaveGraphicsState,                             InterfaceObject::CmdBuffer,            "CmdSaveGraphicsState"                    },
    { InterfaceFunc::CmdBufferCmdRestoreGraphicsState,                          InterfaceObject::CmdBuffer,            "CmdRestoreGraphicsState"                 },
    { InterfaceFunc::CmdBufferCmdBindColorBlendState,                           InterfaceObject::CmdBuffer,            "CmdBindColorBlendState"                  },
    { InterfaceFunc::CmdBufferCmdBindDepthStencilState,                         InterfaceObject::CmdBuffer,            "CmdBindDepthStencilState"                },
    { InterfaceFunc::CmdBufferCmdSetDepthBounds,                                InterfaceObject::CmdBuffer,            "CmdSetDepthBounds"                       },
    { InterfaceFunc::CmdBufferCmdSetUserData,                                   InterfaceObject::CmdBuffer,            "CmdSetUserData"                          },
    { InterfaceFunc::CmdBufferCmdDuplicateUserData,                             InterfaceObject::CmdBuffer,            "CmdDuplicateUserData"                    },
    { InterfaceFunc::CmdBufferCmdSetKernelArguments,                            InterfaceObject::CmdBuffer,            "CmdSetKernelArguments"                   },
    { InterfaceFunc::CmdBufferCmdSetVertexBuffers,                              InterfaceObject::CmdBuffer,            "CmdSetVertexBuffers"                     },
    { InterfaceFunc::CmdBufferCmdBindIndexData,                                 InterfaceObject::CmdBuffer,            "CmdBindIndexData"                        },
    { InterfaceFunc::CmdBufferCmdBindTargets,                                   InterfaceObject::CmdBuffer,            "CmdBindTargets"                          },
    { InterfaceFunc::CmdBufferCmdBindStreamOutTargets,                          InterfaceObject::CmdBuffer,            "CmdBindStreamOutTargets"                 },
    { InterfaceFunc::CmdBufferCmdSetPerDrawVrsRate,                             InterfaceObject::CmdBuffer,            "CmdSetPerDrawVrsRate"                    },
    { InterfaceFunc::CmdBufferCmdSetVrsCenterState,                             InterfaceObject::CmdBuffer,            "CmdSetVrsCenterState"                    },
    { InterfaceFunc::CmdBufferCmdBindSampleRateImage,                           InterfaceObject::CmdBuffer,            "CmdBindSampleRateImage"                  },
    { InterfaceFunc::CmdBufferCmdResolvePrtPlusImage,                           InterfaceObject::CmdBuffer,            "CmdResolvePrtPlusImage"                  },
    { InterfaceFunc::CmdBufferCmdSetBlendConst,                                 InterfaceObject::CmdBuffer,            "CmdSetBlendConst"                        },
    { InterfaceFunc::CmdBufferCmdSetInputAssemblyState,                         InterfaceObject::CmdBuffer,            "CmdSetInputAssemblyState"                },
    { InterfaceFunc::CmdBufferCmdSetTriangleRasterState,                        InterfaceObject::CmdBuffer,            "CmdSetTriangleRasterState"               },
    { InterfaceFunc::CmdBufferCmdSetPointLineRasterState,                       InterfaceObject::CmdBuffer,            "CmdSetPointLineRasterState"              },
    { InterfaceFunc::CmdBufferCmdSetLineStippleState,                           InterfaceObject::CmdBuffer,            "CmdSetLineStippleState"                  },
    { InterfaceFunc::CmdBufferCmdSetDepthBiasState,                             InterfaceObject::CmdBuffer,            "CmdSetDepthBiasState"                    },
    { InterfaceFunc::CmdBufferCmdSetStencilRefMasks,                            InterfaceObject::CmdBuffer,            "CmdSetStencilRefMasks"                   },
    { InterfaceFunc::CmdBufferCmdSetUserClipPlanes,                             InterfaceObject::CmdBuffer,            "CmdSetUserClipPlanes"                    },
    { InterfaceFunc::CmdBufferCmdSetMsaaQuadSamplePattern,                      InterfaceObject::CmdBuffer,            "CmdSetMsaaQuadSamplePattern"             },
    { InterfaceFunc::CmdBufferCmdSetViewports,                                  InterfaceObject::CmdBuffer,            "CmdSetViewports"                         },
    { InterfaceFunc::CmdBufferCmdSetScissorRects,                               InterfaceObject::CmdBuffer,            "CmdSetScissorRects"                      },
    { InterfaceFunc::CmdBufferCmdSetGlobalScissor,                              InterfaceObject::CmdBuffer,            "CmdSetGlobalScissor"                     },
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 778
    { InterfaceFunc::CmdBufferCmdSetColorWriteMask,                             InterfaceObject::CmdBuffer,            "CmdSetColorWriteMask"                    },
    { InterfaceFunc::CmdBufferCmdSetRasterizerDiscardEnable,                    InterfaceObject::CmdBuffer,            "CmdSetRasterizerDiscardEnable"           },
#endif
    { InterfaceFunc::CmdBufferCmdBarrier,                                       InterfaceObject::CmdBuffer,            "CmdBarrier"                              },
    { InterfaceFunc::CmdBufferCmdRelease,                                       InterfaceObject::CmdBuffer,            "CmdRelease"                              },
    { InterfaceFunc::CmdBufferCmdAcquire,                                       InterfaceObject::CmdBuffer,            "CmdAcquire"                              },
    { InterfaceFunc::CmdBufferCmdReleaseEvent,                                  InterfaceObject::CmdBuffer,            "CmdReleaseEvent"                         },
    { InterfaceFunc::CmdBufferCmdAcquireEvent,                                  InterfaceObject::CmdBuffer,            "CmdAcquireEvent"                         },
    { InterfaceFunc::CmdBufferCmdReleaseThenAcquire,                            InterfaceObject::CmdBuffer,            "CmdReleaseThenAcquire"                   },
    { InterfaceFunc::CmdBufferCmdDraw,                                          InterfaceObject::CmdBuffer,            "CmdDraw"                                 },
    { InterfaceFunc::CmdBufferCmdDrawOpaque,                                    InterfaceObject::CmdBuffer,            "CmdDrawOpaque"                           },
    { InterfaceFunc::CmdBufferCmdDrawIndexed,                                   InterfaceObject::CmdBuffer,            "CmdDrawIndexed"                          },
    { InterfaceFunc::CmdBufferCmdDrawIndirectMulti,                             InterfaceObject::CmdBuffer,            "CmdDrawIndirectMulti"                    },
    { InterfaceFunc::CmdBufferCmdDrawIndexedIndirectMulti,                      InterfaceObject::CmdBuffer,            "CmdDrawIndexedIndirectMulti"             },
    { InterfaceFunc::CmdBufferCmdDispatch,                                      InterfaceObject::CmdBuffer,            "CmdDispatch"                             },
    { InterfaceFunc::CmdBufferCmdDispatchIndirect,                              InterfaceObject::CmdBuffer,            "CmdDispatchIndirect"                     },
    { InterfaceFunc::CmdBufferCmdDispatchOffset,                                InterfaceObject::CmdBuffer,            "CmdDispatchOffset"                       },
    { InterfaceFunc::CmdBufferCmdDispatchDynamic,                               InterfaceObject::CmdBuffer,            "CmdDispatchDynamic"                      },
    { InterfaceFunc::CmdBufferCmdDispatchMesh,                                  InterfaceObject::CmdBuffer,            "CmdDispatchMesh"                         },
    { InterfaceFunc::CmdBufferCmdDispatchMeshIndirectMulti,                     InterfaceObject::CmdBuffer,            "CmdDispatchMeshIndirectMulti",           },
    { InterfaceFunc::CmdBufferCmdCopyMemory,                                    InterfaceObject::CmdBuffer,            "CmdCopyMemory"                           },
    { InterfaceFunc::CmdBufferCmdCopyMemoryByGpuVa,                             InterfaceObject::CmdBuffer,            "CmdCopyMemoryByGpuVa"                    },
    { InterfaceFunc::CmdBufferCmdCopyImage,                                     InterfaceObject::CmdBuffer,            "CmdCopyImage"                            },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToImage,                             InterfaceObject::CmdBuffer,            "CmdCopyMemoryToImage"                    },
    { InterfaceFunc::CmdBufferCmdCopyImageToMemory,                             InterfaceObject::CmdBuffer,            "CmdCopyImageToMemory"                    },
    { InterfaceFunc::CmdBufferCmdCopyMemoryToTiledImage,                        InterfaceObject::CmdBuffer,            "CmdCopyMemoryToTiledImage"               },
    { InterfaceFunc::CmdBufferCmdCopyTiledImageToMemory,                        InterfaceObject::CmdBuffer,            "CmdCopyTiledImageToMemory"               },
    { InterfaceFunc::CmdBufferCmdCopyTypedBuffer,                               InterfaceObject::CmdBuffer,            "CmdCopyTypedBuffer"                      },
    { InterfaceFunc::CmdBufferCmdCopyRegisterToMemory,                          InterfaceObject::CmdBuffer,            "CmdCopyRegisterToMemory"                 },
    { InterfaceFunc::CmdBufferCmdScaledCopyImage,                               InterfaceObject::CmdBuffer,            "CmdScaledCopyImage"                      },
    { InterfaceFunc::CmdBufferCmdGenerateMipmaps,                               InterfaceObject::CmdBuffer,            "CmdGenerateMipmaps"                      },
    { InterfaceFunc::CmdBufferCmdColorSpaceConversionCopy,                      InterfaceObject::CmdBuffer,            "CmdColorSpaceConversionCopy"             },
    { InterfaceFunc::CmdBufferCmdCloneImageData,                                InterfaceObject::CmdBuffer,            "CmdCloneImageData"                       },
    { InterfaceFunc::CmdBufferCmdUpdateMemory,                                  InterfaceObject::CmdBuffer,            "CmdUpdateMemory"                         },
    { InterfaceFunc::CmdBufferCmdUpdateBusAddressableMemoryMarker,              InterfaceObject::CmdBuffer,            "CmdUpdateBusAddressableMemoryMarker"     },
    { InterfaceFunc::CmdBufferCmdFillMemory,                                    InterfaceObject::CmdBuffer,            "CmdFillMemory"                           },
    { InterfaceFunc::CmdBufferCmdClearColorBuffer,                              InterfaceObject::CmdBuffer,            "CmdClearColorBuffer"                     },
    { InterfaceFunc::CmdBufferCmdClearBoundColorTargets,                        InterfaceObject::CmdBuffer,            "CmdClearBoundColorTargets"               },
    { InterfaceFunc::CmdBufferCmdClearColorImage,                               InterfaceObject::CmdBuffer,            "CmdClearColorImage"                      },
    { InterfaceFunc::CmdBufferCmdClearBoundDepthStencilTargets,                 InterfaceObject::CmdBuffer,            "CmdClearBoundDepthStencilTargets"        },
    { InterfaceFunc::CmdBufferCmdClearDepthStencil,                             InterfaceObject::CmdBuffer,            "CmdClearDepthStencil"                    },
    { InterfaceFunc::CmdBufferCmdClearBufferView,                               InterfaceObject::CmdBuffer,            "CmdClearBufferView"                      },
    { InterfaceFunc::CmdBufferCmdClearImageView,                                InterfaceObject::CmdBuffer,            "CmdClearImageView"                       },
    { InterfaceFunc::CmdBufferCmdResolveImage,                                  InterfaceObject::CmdBuffer,            "CmdResolveImage"                         },
    { InterfaceFunc::CmdBufferCmdSetEvent,                                      InterfaceObject::CmdBuffer,            "CmdSetEvent"                             },
    { InterfaceFunc::CmdBufferCmdResetEvent,                                    InterfaceObject::CmdBuffer,            "CmdResetEvent"                           },
    { InterfaceFunc::CmdBufferCmdPredicateEvent,                                InterfaceObject::CmdBuffer,            "CmdPredicateEvent"                       },
    { InterfaceFunc::CmdBufferCmdMemoryAtomic,                                  InterfaceObject::CmdBuffer,            "CmdMemoryAtomic"                         },
    { InterfaceFunc::CmdBufferCmdBeginQuery,                                    InterfaceObject::CmdBuffer,            "CmdBeginQuery"                           },
    { InterfaceFunc::CmdBufferCmdEndQuery,                                      InterfaceObject::CmdBuffer,            "CmdEndQuery"                             },
    { InterfaceFunc::CmdBufferCmdResolveQuery,                                  InterfaceObject::CmdBuffer,            "CmdResolveQuery"                         },
    { InterfaceFunc::CmdBufferCmdResetQueryPool,                                InterfaceObject::CmdBuffer,            "CmdResetQueryPool"                       },
    { InterfaceFunc::CmdBufferCmdWriteTimestamp,                                InterfaceObject::CmdBuffer,            "CmdWriteTimestamp"                       },
    { InterfaceFunc::CmdBufferCmdWriteImmediate,                                InterfaceObject::CmdBuffer,            "CmdWriteImmediate"                       },
    { InterfaceFunc::CmdBufferCmdLoadBufferFilledSizes,                         InterfaceObject::CmdBuffer,            "CmdLoadBufferFilledSizes"                },
    { InterfaceFunc::CmdBufferCmdSaveBufferFilledSizes,                         InterfaceObject::CmdBuffer,            "CmdSaveBufferFilledSizes"                },
    { InterfaceFunc::CmdBufferCmdSetBufferFilledSize,                           InterfaceObject::CmdBuffer,            "CmdSetBufferFilledSize"                  },
    { InterfaceFunc::CmdBufferCmdBindBorderColorPalette,                        InterfaceObject::CmdBuffer,            "CmdBindBorderColorPalette"               },
    { InterfaceFunc::CmdBufferCmdSetPredication,                                InterfaceObject::CmdBuffer,            "CmdSetPredication"                       },
    { InterfaceFunc::CmdBufferCmdSuspendPredication,                            InterfaceObject::CmdBuffer,            "CmdSuspendPredication"                   },
    { InterfaceFunc::CmdBufferCmdIf,                                            InterfaceObject::CmdBuffer,            "CmdIf"                                   },
    { InterfaceFunc::CmdBufferCmdElse,                                          InterfaceObject::CmdBuffer,            "CmdElse"                                 },
    { InterfaceFunc::CmdBufferCmdEndIf,                                         InterfaceObject::CmdBuffer,            "CmdEndIf"                                },
    { InterfaceFunc::CmdBufferCmdWhile,                                         InterfaceObject::CmdBuffer,            "CmdWhile"                                },
    { InterfaceFunc::CmdBufferCmdEndWhile,                                      InterfaceObject::CmdBuffer,            "CmdEndWhile"                             },
    { InterfaceFunc::CmdBufferCmdWaitRegisterValue,                             InterfaceObject::CmdBuffer,            "CmdWaitRegisterValue"                    },
    { InterfaceFunc::CmdBufferCmdWaitMemoryValue,                               InterfaceObject::CmdBuffer,            "CmdWaitMemoryValue"                      },
    { InterfaceFunc::CmdBufferCmdWaitBusAddressableMemoryMarker,                InterfaceObject::CmdBuffer,            "CmdWaitBusAddressableMemoryMarker"       },
    { InterfaceFunc::CmdBufferCmdLoadCeRam,                                     InterfaceObject::CmdBuffer,            "CmdLoadCeRam"                            },
    { InterfaceFunc::CmdBufferCmdDumpCeRam,                                     InterfaceObject::CmdBuffer,            "CmdDumpCeRam"                            },
    { InterfaceFunc::CmdBufferCmdWriteCeRam,                                    InterfaceObject::CmdBuffer,            "CmdWriteCeRam"                           },
    { InterfaceFunc::CmdBufferCmdAllocateEmbeddedData,                          InterfaceObject::CmdBuffer,            "CmdAllocateEmbeddedData"                 },
    { InterfaceFunc::CmdBufferCmdExecuteNestedCmdBuffers,                       InterfaceObject::CmdBuffer,            "CmdExecuteNestedCmdBuffers"              },
    { InterfaceFunc::CmdBufferCmdSaveComputeState,                              InterfaceObject::CmdBuffer,            "CmdSaveComputeState"                     },
    { InterfaceFunc::CmdBufferCmdRestoreComputeState,                           InterfaceObject::CmdBuffer,            "CmdRestoreComputeState"                  },
    { InterfaceFunc::CmdBufferCmdExecuteIndirectCmds,                           InterfaceObject::CmdBuffer,            "CmdExecuteIndirectCmds"                  },
    { InterfaceFunc::CmdBufferCmdSetMarker,                                     InterfaceObject::CmdBuffer,            "CmdSetMarker"                            },
    { InterfaceFunc::CmdBufferCmdPresent,                                       InterfaceObject::CmdBuffer,            "CmdPresent"                              },
    { InterfaceFunc::CmdBufferCmdCommentString,                                 InterfaceObject::CmdBuffer,            "CmdCommentString"                        },
    { InterfaceFunc::CmdBufferCmdNop,                                           InterfaceObject::CmdBuffer,            "CmdNop"                                  },
    { InterfaceFunc::CmdBufferCmdXdmaWaitFlipPending,                           InterfaceObject::CmdBuffer,            "CmdXdmaWaitFlipPending"                  },
    { InterfaceFunc::CmdBufferCmdStartGpuProfilerLogging,                       InterfaceObject::CmdBuffer,            "CmdStartGpuProfilerLogging"              },
    { InterfaceFunc::CmdBufferCmdStopGpuProfilerLogging,                        InterfaceObject::CmdBuffer,            "CmdStopGpuProfilerLogging"               },
    { InterfaceFunc::CmdBufferDestroy,                                          InterfaceObject::CmdBuffer,            "Destroy"                                 },
    { InterfaceFunc::CmdBufferCmdSetViewInstanceMask,                           InterfaceObject::CmdBuffer,            "CmdSetViewInstanceMask"                  },
    { InterfaceFunc::CmdUpdateHiSPretests,                                      InterfaceObject::CmdBuffer,            "CmdUpdateHiSPretests"                    },
    { InterfaceFunc::CmdBufferCmdSetClipRects,                                  InterfaceObject::CmdBuffer,            "CmdSetClipRects"                         },
    { InterfaceFunc::CmdBufferCmdPostProcessFrame,                              InterfaceObject::CmdBuffer,            "CmdBufferCmdPostProcessFrame"            },
    { InterfaceFunc::ColorBlendStateDestroy,                                    InterfaceObject::ColorBlendState,      "Destroy"                                 },
    { InterfaceFunc::DepthStencilStateDestroy,                                  InterfaceObject::DepthStencilState,    "Destroy"                                 },
    { InterfaceFunc::DeviceCommitSettingsAndInit,                               InterfaceObject::Device,               "CommitSettingsAndInit"                   },
    { InterfaceFunc::DeviceFinalize,                                            InterfaceObject::Device,               "Finalize"                                },
    { InterfaceFunc::DeviceCleanup,                                             InterfaceObject::Device,               "Cleanup"                                 },
    { InterfaceFunc::DeviceSetMaxQueuedFrames,                                  InterfaceObject::Device,               "SetMaxQueuedFrames"                      },
    { InterfaceFunc::DeviceAddGpuMemoryReferences,                              InterfaceObject::Device,               "AddGpuMemoryReferences"                  },
    { InterfaceFunc::DeviceRemoveGpuMemoryReferences,                           InterfaceObject::Device,               "RemoveGpuMemoryReferences"               },
    { InterfaceFunc::DeviceSetClockMode,                                        InterfaceObject::Device,               "SetClockMode"                            },
    { InterfaceFunc::DeviceSetMgpuMode,                                         InterfaceObject::Device,               "SetMgpuMode"                             },
    { InterfaceFunc::DeviceOfferAllocations,                                    InterfaceObject::Device,               "OfferAllocations"                        },
    { InterfaceFunc::DeviceReclaimAllocations,                                  InterfaceObject::Device,               "ReclaimAllocations"                      },
    { InterfaceFunc::DeviceResetFences,                                         InterfaceObject::Device,               "ResetFences"                             },
    { InterfaceFunc::DeviceWaitForFences,                                       InterfaceObject::Device,               "WaitForFences"                           },
    { InterfaceFunc::DeviceBindTrapHandler,                                     InterfaceObject::Device,               "BindTrapHandler"                         },
    { InterfaceFunc::DeviceBindTrapBuffer,                                      InterfaceObject::Device,               "BindTrapBuffer"                          },
    { InterfaceFunc::DeviceCreateQueue,                                         InterfaceObject::Device,               "CreateQueue"                             },
    { InterfaceFunc::DeviceCreateMultiQueue,                                    InterfaceObject::Device,               "CreateMultiQueue"                        },
    { InterfaceFunc::DeviceCreateGpuMemory,                                     InterfaceObject::Device,               "CreateGpuMemory"                         },
    { InterfaceFunc::DeviceCreatePinnedGpuMemory,                               InterfaceObject::Device,               "CreatePinnedGpuMemory"                   },
    { InterfaceFunc::DeviceCreateSvmGpuMemory,                                  InterfaceObject::Device,               "CreateSvmGpuMemory"                      },
    { InterfaceFunc::DeviceOpenSharedGpuMemory,                                 InterfaceObject::Device,               "OpenSharedGpuMemory"                     },
    { InterfaceFunc::DeviceOpenExternalSharedGpuMemory,                         InterfaceObject::Device,               "OpenExternalSharedGpuMemory"             },
    { InterfaceFunc::DeviceOpenPeerGpuMemory,                                   InterfaceObject::Device,               "OpenPeerGpuMemory"                       },
    { InterfaceFunc::DeviceCreateImage,                                         InterfaceObject::Device,               "CreateImage"                             },
    { InterfaceFunc::DeviceCreatePresentableImage,                              InterfaceObject::Device,               "CreatePresentableImage"                  },
    { InterfaceFunc::DeviceOpenPeerImage,                                       InterfaceObject::Device,               "OpenPeerImage"                           },
    { InterfaceFunc::DeviceOpenExternalSharedImage,                             InterfaceObject::Device,               "OpenExternalSharedImage"                 },
    { InterfaceFunc::DeviceCreateColorTargetView,                               InterfaceObject::Device,               "CreateColorTargetView"                   },
    { InterfaceFunc::DeviceCreateDepthStencilView,                              InterfaceObject::Device,               "CreateDepthStencilView"                  },
    { InterfaceFunc::DeviceCreateTypedBufferViewSrds,                           InterfaceObject::Device,               "CreateTypedBufferViewSrds"               },
    { InterfaceFunc::DeviceCreateUntypedBufferViewSrds,                         InterfaceObject::Device,               "CreateUntypedBufferViewSrds"             },
    { InterfaceFunc::DeviceCreateImageViewSrds,                                 InterfaceObject::Device,               "CreateImageViewSrds"                     },
    { InterfaceFunc::DeviceCreateFmaskViewSrds,                                 InterfaceObject::Device,               "CreateFmaskViewSrds"                     },
    { InterfaceFunc::DeviceCreateSamplerSrds,                                   InterfaceObject::Device,               "CreateSamplerSrds"                       },
    { InterfaceFunc::DeviceCreateBvhSrds,                                       InterfaceObject::Device,               "CreateBvhSrds"                           },
    { InterfaceFunc::DeviceSetSamplePatternPalette,                             InterfaceObject::Device,               "SetSamplePatternPalette"                 },
    { InterfaceFunc::DeviceCreateBorderColorPalette,                            InterfaceObject::Device,               "CreateBorderColorPalette"                },
    { InterfaceFunc::DeviceCreateComputePipeline,                               InterfaceObject::Device,               "CreateComputePipeline"                   },
    { InterfaceFunc::DeviceCreateGraphicsPipeline,                              InterfaceObject::Device,               "CreateGraphicsPipeline"                  },
    { InterfaceFunc::DeviceCreateShaderLibrary,                                 InterfaceObject::Device,               "CreateShaderLibrary"                     },
    { InterfaceFunc::DeviceCreateMsaaState,                                     InterfaceObject::Device,               "CreateMsaaState"                         },
    { InterfaceFunc::DeviceCreateColorBlendState,                               InterfaceObject::Device,               "CreateColorBlendState"                   },
    { InterfaceFunc::DeviceCreateDepthStencilState,                             InterfaceObject::Device,               "CreateDepthStencilState"                 },
    { InterfaceFunc::DeviceCreateQueueSemaphore,                                InterfaceObject::Device,               "CreateQueueSemaphore"                    },
    { InterfaceFunc::DeviceOpenSharedQueueSemaphore,                            InterfaceObject::Device,               "OpenSharedQueueSemaphore"                },
    { InterfaceFunc::DeviceOpenExternalSharedQueueSemaphore,                    InterfaceObject::Device,               "OpenExternalSharedQueueSemaphore"        },
    { InterfaceFunc::DeviceCreateFence,                                         InterfaceObject::Device,               "CreateFence"                             },
    { InterfaceFunc::DeviceOpenFence,                                           InterfaceObject::Device,               "OpenFence"                               },
    { InterfaceFunc::DeviceCreateGpuEvent,                                      InterfaceObject::Device,               "CreateGpuEvent"                          },
    { InterfaceFunc::DeviceCreateQueryPool,                                     InterfaceObject::Device,               "CreateQueryPool"                         },
    { InterfaceFunc::DeviceCreateCmdAllocator,                                  InterfaceObject::Device,               "CreateCmdAllocator"                      },
    { InterfaceFunc::DeviceCreateCmdBuffer,                                     InterfaceObject::Device,               "CreateCmdBuffer"                         },
    { InterfaceFunc::DeviceCreateIndirectCmdGenerator,                          InterfaceObject::Device,               "CreateIndirectCmdGenerator"              },
    { InterfaceFunc::DeviceGetPrivateScreens,                                   InterfaceObject::Device,               "GetPrivateScreens"                       },
    { InterfaceFunc::DeviceAddEmulatedPrivateScreen,                            InterfaceObject::Device,               "AddEmulatedPrivateScreen"                },
    { InterfaceFunc::DeviceRemoveEmulatedPrivateScreen,                         InterfaceObject::Device,               "RemoveEmulatedPrivateScreen"             },
    { InterfaceFunc::DeviceCreatePrivateScreenImage,                            InterfaceObject::Device,               "CreatePrivateScreenImage"                },
    { InterfaceFunc::DeviceCreateSwapChain,                                     InterfaceObject::Device,               "CreateSwapChain"                         },
    { InterfaceFunc::DeviceSetPowerProfile,                                     InterfaceObject::Device,               "SetPowerProfile"                         },
    { InterfaceFunc::DeviceFlglQueryState,                                      InterfaceObject::Device,               "FlglQueryState"                          },
    { InterfaceFunc::DeviceFlglSetSyncConfiguration,                            InterfaceObject::Device,               "FlglSetSyncConfiguration"                },
    { InterfaceFunc::DeviceFlglGetSyncConfiguration,                            InterfaceObject::Device,               "FlglGetSyncConfiguration"                },
    { InterfaceFunc::DeviceFlglSetFrameLock,                                    InterfaceObject::Device,               "FlglSetFrameLock"                        },
    { InterfaceFunc::DeviceFlglSetGenLock,                                      InterfaceObject::Device,               "FlglSetGenLock"                          },
    { InterfaceFunc::DeviceFlglResetFrameCounter,                               InterfaceObject::Device,               "FlglResetFrameCounter"                   },
    { InterfaceFunc::DeviceFlglGetFrameCounter,                                 InterfaceObject::Device,               "FlglGetFrameCounter"                     },
    { InterfaceFunc::DeviceFlglGetFrameCounterResetStatus,                      InterfaceObject::Device,               "FlglGetFrameCounterResetStatus"          },
    { InterfaceFunc::DeviceCreateVirtualDisplay,                                InterfaceObject::Device,               "CreateVirtualDisplay"                    },
    { InterfaceFunc::DeviceDestroyVirtualDisplay,                               InterfaceObject::Device,               "DestroyVirtualDisplay"                   },
    { InterfaceFunc::DeviceGetVirtualDisplayProperties,                         InterfaceObject::Device,               "GetVirtualDisplayProperties"             },
    { InterfaceFunc::FenceDestroy,                                              InterfaceObject::Fence,                "Destroy"                                 },
    { InterfaceFunc::GpuEventSet,                                               InterfaceObject::GpuEvent,             "Set"                                     },
    { InterfaceFunc::GpuEventReset,                                             InterfaceObject::GpuEvent,             "Reset"                                   },
    { InterfaceFunc::GpuEventBindGpuMemory,                                     InterfaceObject::GpuEvent,             "BindGpuMemory"                           },
    { InterfaceFunc::GpuEventDestroy,                                           InterfaceObject::GpuEvent,             "Destroy"                                 },
    { InterfaceFunc::GpuMemorySetPriority,                                      InterfaceObject::GpuMemory,            "SetPriority"                             },
    { InterfaceFunc::GpuMemoryMap,                                              InterfaceObject::GpuMemory,            "Map"                                     },
    { InterfaceFunc::GpuMemoryUnmap,                                            InterfaceObject::GpuMemory,            "Unmap"                                   },
    { InterfaceFunc::GpuMemorySetSdiRemoteBusAddress,                           InterfaceObject::GpuMemory,            "SetSdiRemoteBusAddress"                  },
    { InterfaceFunc::GpuMemoryDestroy,                                          InterfaceObject::GpuMemory,            "Destroy"                                 },
    { InterfaceFunc::ImageBindGpuMemory,                                        InterfaceObject::Image,                "BindGpuMemory"                           },
    { InterfaceFunc::ImageDestroy,                                              InterfaceObject::Image,                "Destroy"                                 },
    { InterfaceFunc::IndirectCmdGeneratorBindGpuMemory,                         InterfaceObject::IndirectCmdGenerator, "BindGpuMemory"                           },
    { InterfaceFunc::IndirectCmdGeneratorDestroy,                               InterfaceObject::IndirectCmdGenerator, "Destroy"                                 },
    { InterfaceFunc::MsaaStateDestroy,                                          InterfaceObject::MsaaState,            "Destroy"                                 },
    { InterfaceFunc::PipelineCreateLaunchDescriptor,                            InterfaceObject::Pipeline,             "CreateLaunchDescriptor"                  },
    { InterfaceFunc::PipelineLinkWithLibraries,                                 InterfaceObject::Pipeline,             "LinkWithLibraries"                       },
    { InterfaceFunc::PipelineDestroy,                                           InterfaceObject::Pipeline,             "Destroy"                                 },
    { InterfaceFunc::PlatformEnumerateDevices,                                  InterfaceObject::Platform,             "EnumerateDevices"                        },
    { InterfaceFunc::PlatformGetScreens,                                        InterfaceObject::Platform,             "GetScreens"                              },
    { InterfaceFunc::PlatformTurboSyncControl,                                  InterfaceObject::Platform,             "TurboSyncControl"                        },
    { InterfaceFunc::PlatformDestroy,                                           InterfaceObject::Platform,             "Destroy"                                 },
    { InterfaceFunc::PrivateScreenEnable,                                       InterfaceObject::PrivateScreen,        "Enable"                                  },
    { InterfaceFunc::PrivateScreenDisable,                                      InterfaceObject::PrivateScreen,        "Disable"                                 },
    { InterfaceFunc::PrivateScreenBlank,                                        InterfaceObject::PrivateScreen,        "Blank"                                   },
    { InterfaceFunc::PrivateScreenPresent,                                      InterfaceObject::PrivateScreen,        "Present"                                 },
    { InterfaceFunc::PrivateScreenSetGammaRamp,                                 InterfaceObject::PrivateScreen,        "SetGammaRamp"                            },
    { InterfaceFunc::PrivateScreenSetPowerMode,                                 InterfaceObject::PrivateScreen,        "SetPowerMode"                            },
    { InterfaceFunc::PrivateScreenSetDisplayMode,                               InterfaceObject::PrivateScreen,        "SetDisplayMode"                          },
    { InterfaceFunc::PrivateScreenSetColorMatrix,                               InterfaceObject::PrivateScreen,        "SetColorMatrix"                          },
    { InterfaceFunc::PrivateScreenSetEventAfterVsync,                           InterfaceObject::PrivateScreen,        "SetEventAfterVsync"                      },
    { InterfaceFunc::PrivateScreenEnableAudio,                                  InterfaceObject::PrivateScreen,        "EnableAudio"                             },
    { InterfaceFunc::QueryPoolBindGpuMemory,                                    InterfaceObject::QueryPool,            "BindGpuMemory"                           },
    { InterfaceFunc::QueryPoolDestroy,                                          InterfaceObject::QueryPool,            "Destroy"                                 },
    { InterfaceFunc::QueryPoolReset,                                            InterfaceObject::QueryPool,            "Reset"                                   },
    { InterfaceFunc::QueueSubmit,                                               InterfaceObject::Queue,                "Submit"                                  },
    { InterfaceFunc::QueueWaitIdle,                                             InterfaceObject::Queue,                "WaitIdle"                                },
    { InterfaceFunc::QueueSignalQueueSemaphore,                                 InterfaceObject::Queue,                "SignalQueueSemaphore"                    },
    { InterfaceFunc::QueueWaitQueueSemaphore,                                   InterfaceObject::Queue,                "WaitQueueSemaphore"                      },
    { InterfaceFunc::QueuePresentDirect,                                        InterfaceObject::Queue,                "PresentDirect"                           },
    { InterfaceFunc::QueuePresentSwapChain,                                     InterfaceObject::Queue,                "PresentSwapChain"                        },
    { InterfaceFunc::QueueDelay,                                                InterfaceObject::Queue,                "Delay"                                   },
    { InterfaceFunc::QueueDelayAfterVsync,                                      InterfaceObject::Queue,                "DelayAfterVsync"                         },
    { InterfaceFunc::QueueRemapVirtualMemoryPages,                              InterfaceObject::Queue,                "RemapVirtualMemoryPages"                 },
    { InterfaceFunc::QueueCopyVirtualMemoryPageMappings,                        InterfaceObject::Queue,                "CopyVirtualMemoryPageMappings"           },
    { InterfaceFunc::QueueAssociateFenceWithLastSubmit,                         InterfaceObject::Queue,                "AssociateFenceWithLastSubmit"            },
    { InterfaceFunc::QueueSetExecutionPriority,                                 InterfaceObject::Queue,                "SetExecutionPriority"                    },
    { InterfaceFunc::QueueDestroy,                                              InterfaceObject::Queue,                "Destroy"                                 },
    { InterfaceFunc::QueueSemaphoreDestroy,                                     InterfaceObject::QueueSemaphore,       "Destroy"                                 },
    { InterfaceFunc::ScreenIsImplicitFullscreenOwnershipSafe,                   InterfaceObject::Screen,               "IsImplicitFullscreenOwnershipSafe"       },
    { InterfaceFunc::ScreenQueryCurrentDisplayMode,                             InterfaceObject::Screen,               "QueryCurrentDisplayMode"                 },
    { InterfaceFunc::ScreenTakeFullscreenOwnership,                             InterfaceObject::Screen,               "TakeFullscreenOwnership"                 },
    { InterfaceFunc::ScreenReleaseFullscreenOwnership,                          InterfaceObject::Screen,               "ReleaseFullscreenOwnership"              },
    { InterfaceFunc::ScreenSetGammaRamp,                                        InterfaceObject::Screen,               "SetGammaRamp"                            },
    { InterfaceFunc::ScreenWaitForVerticalBlank,                                InterfaceObject::Screen,               "WaitForVerticalBlank"                    },
    { InterfaceFunc::ScreenDestroy,                                             InterfaceObject::Screen,               "Destroy"                                 },
    { InterfaceFunc::ShaderLibraryDestroy,                                      InterfaceObject::ShaderLibrary,        "Destroy"                                 },
    { InterfaceFunc::SwapChainAcquireNextImage,                                 InterfaceObject::SwapChain,            "AcquireNextImage"                        },
    { InterfaceFunc::SwapChainWaitIdle,                                         InterfaceObject::SwapChain,            "WaitIdle"                                },
    { InterfaceFunc::SwapChainDestroy,                                          InterfaceObject::SwapChain,            "Destroy"                                 },
};

static_assert(ArrayLen(FuncFormattingTable) == static_cast<size_t>(InterfaceFunc::Count),
              "The FuncFormattingTable must be updated.");

// =====================================================================================================================
LogStream::LogStream(
    Platform* pPlatform)
    :
    m_pPlatform(pPlatform),
    m_pBuffer(nullptr),
    m_bufferSize(0),
    m_bufferUsed(0)
{
}

// =====================================================================================================================
LogStream::~LogStream()
{
    if (m_file.IsOpen())
    {
        // Write out anything left in the buffer. If the file was never opened nothing gets written.
        const Result result = WriteFile();
        PAL_ASSERT(result == Result::Success);
    }

    PAL_SAFE_FREE(m_pBuffer, m_pPlatform);
}

// =====================================================================================================================
Result LogStream::OpenFile(
    const char* pFilePath)
{
    Result result = m_file.Open(pFilePath, Util::FileAccessWrite);

    if (result == Result::Success)
    {
        // Write out anything that was logged before now.
        result = WriteFile();
    }

    return result;
}

// =====================================================================================================================
Result LogStream::WriteFile()
{
    Result result = Result::Success;

    if (m_file.IsOpen() == false)
    {
        result = Result::ErrorUnavailable;
    }
    else if (m_bufferUsed > 0)
    {
        result       = m_file.Write(m_pBuffer, m_bufferUsed * sizeof(char));
        m_bufferUsed = 0;

        if (result == Result::Success)
        {
            // Flush to disk to make the logs more useful if the application crashes.
            result = m_file.Flush();
        }
    }

    return result;
}

// =====================================================================================================================
void LogStream::WriteString(
    const char* pString,
    uint32      length)
{
    VerifyUnusedSpace(length);
    memcpy(m_pBuffer + m_bufferUsed, pString, length * sizeof(char));
    m_bufferUsed += length;
}

// =====================================================================================================================
void LogStream::WriteCharacter(
    char character)
{
    VerifyUnusedSpace(1);
    m_pBuffer[m_bufferUsed++] = character;
}

// =====================================================================================================================
// Verifies that the buffer has enough space for an additional "size" characters, reallocating if necessary.
void LogStream::VerifyUnusedSpace(
    uint32 size)
{
    if (m_bufferSize - m_bufferUsed < size)
    {
        const char* pOldBuffer = m_pBuffer;

        // Bump up the size of the buffer to the next multiple of 4K that fits the current contents plus "size".
        m_bufferSize = Pow2Align(m_bufferSize + size, 4096);
        m_pBuffer     = static_cast<char*>(PAL_MALLOC(m_bufferSize * sizeof(char), m_pPlatform, AllocInternal));

        PAL_ASSERT(m_pBuffer != nullptr);

        memcpy(m_pBuffer, pOldBuffer, m_bufferUsed);
        PAL_SAFE_FREE(pOldBuffer, m_pPlatform);
    }
}

// =====================================================================================================================
LogContext::LogContext(
    Platform* pPlatform)
    :
    JsonWriter(&m_stream),
    m_stream(pPlatform)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    for (uint32 idx = 0; idx < static_cast<uint32>(InterfaceFunc::Count); ++idx)
    {
        PAL_ASSERT(static_cast<uint32>(FuncFormattingTable[idx].function) == idx);
    }
#endif

    // All top-level entries in the log will be contained in a list. If we don't do this, we can only write one entry!
    BeginList(false);
}

// =====================================================================================================================
LogContext::~LogContext()
{
    // End the list we started in the constructor.
    EndList();
}

// =====================================================================================================================
void LogContext::BeginFunc(
    const BeginFuncInfo& info,
    uint32               threadId)
{
    auto const& funcData = FuncFormattingTable[static_cast<uint32>(info.funcId)];

    BeginMap(false);
    KeyAndValue("_type", "InterfaceFunc");
    Key("this");
    Object(funcData.objectType, info.objectId);
    KeyAndValue("name", funcData.pFuncName);
    KeyAndValue("thread", threadId);
    KeyAndValue("preCallTime", info.preCallTime);
    KeyAndValue("postCallTime", info.postCallTime);
}

// =====================================================================================================================
void LogContext::EndFunc()
{
    EndMap();

    // Flush our buffered JSON text to our log file if it's already been opened.
    if (m_stream.IsFileOpen())
    {
        const Result result = m_stream.WriteFile();
        PAL_ASSERT(result == Result::Success);
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IBorderColorPalette* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::BorderColorPalette, static_cast<const BorderColorPalette*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const ICmdAllocator* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::CmdAllocator, static_cast<const CmdAllocator*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const ICmdBuffer* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::CmdBuffer, static_cast<const CmdBuffer*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IColorBlendState* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::ColorBlendState, static_cast<const ColorBlendState*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IColorTargetView* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::ColorTargetView, static_cast<const ColorTargetView*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IDepthStencilState* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::DepthStencilState, static_cast<const DepthStencilState*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IDepthStencilView* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::DepthStencilView, static_cast<const DepthStencilView*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IDevice* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Device, static_cast<const Device*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IFence* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Fence, static_cast<const Fence*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IGpuEvent* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::GpuEvent, static_cast<const GpuEvent*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IGpuMemory* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::GpuMemory, static_cast<const GpuMemory*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IImage* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Image, static_cast<const Image*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IIndirectCmdGenerator* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::IndirectCmdGenerator, static_cast<const IndirectCmdGenerator*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IMsaaState* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::MsaaState, static_cast<const MsaaState*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IPipeline* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Pipeline, static_cast<const Pipeline*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IPrivateScreen* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::PrivateScreen, static_cast<const PrivateScreen*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IQueryPool* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::QueryPool, static_cast<const QueryPool*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IQueue* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Queue, static_cast<const Queue*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IQueueSemaphore* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::QueueSemaphore, static_cast<const QueueSemaphore*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IScreen* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::Screen, static_cast<const Screen*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const IShaderLibrary* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::ShaderLibrary, static_cast<const ShaderLibrary*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    const ISwapChain* pDecorator)
{
    if (pDecorator != nullptr)
    {
        Object(InterfaceObject::SwapChain, static_cast<const SwapChain*>(pDecorator)->ObjectId());
    }
    else
    {
        NullValue();
    }
}

// =====================================================================================================================
void LogContext::Object(
    InterfaceObject objectType,
    uint32          objectId)
{
    BeginMap(true);
    KeyAndValue("class", ObjectNames[static_cast<uint32>(objectType)]);
    KeyAndValue("id", objectId);
    EndMap();
}

// =====================================================================================================================
void LogContext::CacheCoherencyUsageFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "CoherCpu",                // 0x00000001,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 740
        "CoherShader",             // 0x00000002,
        "CoherCopy",               // 0x00000004,
        "CoherColorTarget",        // 0x00000008,
        "CoherDepthStencilTarget", // 0x00000010,
        "CoherResolve",            // 0x00000020,
        "CoherClear",              // 0x00000040,
        "CoherIndirectArgs",       // 0x00000080,
        "CoherIndexData",          // 0x00000100,
        "CoherQueueAtomic",        // 0x00000200,
        "CoherTimestamp",          // 0x00000400,
        "CoherCeLoad",             // 0x00000800,
        "CoherCeDump",             // 0x00001000,
        "CoherStreamOut",          // 0x00002000,
        "CoherMemory",             // 0x00004000,
        "CoherSampleRate",         // 0x00008000,
        "CoherPresent",            // 0x00010000,
#else
        "CoherShaderRead",         // 0x00000002,
        "CoherShaderWrite",        // 0x00000004,
        "CoherCopySrc",            // 0x00000008,
        "CoherCopyDst",            // 0x00000010,
        "CoherColorTarget",        // 0x00000020,
        "CoherDepthStencilTarget", // 0x00000040,
        "CoherResolveSrc",         // 0x00000080,
        "CoherResolveDst",         // 0x00000100,
        "CoherClear",              // 0x00000200,
        "CoherIndirectArgs",       // 0x00000400,
        "CoherIndexData",          // 0x00000800,
        "CoherQueueAtomic",        // 0x00001000,
        "CoherTimestamp",          // 0x00002000,
        "CoherCeLoad",             // 0x00004000,
        "CoherCeDump",             // 0x00008000,
        "CoherStreamOut",          // 0x00010000,
        "CoherMemory",             // 0x00020000,
        "CoherSampleRate",         // 0x00040000,
        "CoherPresent",            // 0x00080000,
#endif
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::PipelineStageFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "PipelineStageTopOfPipe",         //= 0x00000001,
        "PipelineStageFetchIndirectArgs", //= 0x00000002,
        "PipelineStageFetchIndices",      //= 0x00000004,
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 770
        "PipelineStageStreamOut",         //= 0x00000008,
        "PipelineStageVs",                //= 0x00000010,
        "PipelineStageHs",                //= 0x00000020,
        "PipelineStageDs",                //= 0x00000040,
        "PipelineStageGs",                //= 0x00000080,
        "PipelineStagePs",                //= 0x00000100,
        "PipelineStageEarlyDsTarget",     //= 0x00000200,
        "PipelineStageLateDsTarget",      //= 0x00000400,
        "PipelineStageColorTarget",       //= 0x00000800,
        "PipelineStageCs",                //= 0x00001000,
        "PipelineStageBlt",               //= 0x00002000,
        "PipelineStageBottomOfPipe",      //= 0x00004000,
#else
        "PipelineStageVs",                //= 0x00000008,
        "PipelineStageHs",                //= 0x00000010,
        "PipelineStageDs",                //= 0x00000020,
        "PipelineStageGs",                //= 0x00000040,
        "PipelineStagePs",                //= 0x00000080,
        "PipelineStageEarlyDsTarget",     //= 0x00000100,
        "PipelineStageLateDsTarget",      //= 0x00000200,
        "PipelineStageColorTarget",       //= 0x00000400,
        "PipelineStageCs",                //= 0x00000800,
        "PipelineStageBlt",               //= 0x00001000,
        "PipelineStageBottomOfPipe",      //= 0x00002000,
#endif
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ComputeStateFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "ComputeStatePipelineAndUserData", // 0x1,
        "ComputeStateBorderColorPalette",  // 0x2,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::CopyControlFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "CopyFormatConversion",  // 0x1,
        "CopyRawSwizzle",        // 0x2,
        "CopyEnableScissorTest", // 0x4,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::GpuMemoryRefFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "GpuMemoryRefCantTrim",    // 0x1,
        "GpuMemoryRefMustSucceed", // 0x2,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ImageLayoutEngineFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "LayoutUniversalEngine",      // 0x01,
        "LayoutComputeEngine",        // 0x02,
        "LayoutDmaEngine",            // 0x04,
        "LayoutVideoEncodeEngine",    // 0x08,
        "LayoutVideoDecodeEngine",    // 0x10,
        "LayoutVideoJpegDecodeEngine",// 0x20
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ImageLayoutUsageFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "LayoutUninitializedTarget",  // 0x00000001,
        "LayoutColorTarget",          // 0x00000002,
        "LayoutDepthStencilTarget",   // 0x00000004,
        "LayoutShaderRead",           // 0x00000008,
        "LayoutShaderFmaskBasedRead", // 0x00000010,
        "LayoutShaderWrite",          // 0x00000020,
        "LayoutCopySrc",              // 0x00000040,
        "LayoutCopyDst",              // 0x00000080,
        "LayoutResolveSrc",           // 0x00000100,
        "LayoutResolveDst",           // 0x00000200,
        "LayoutPresentWindowed",      // 0x00000400,
        "LayoutPresentFullscreen",    // 0x00000800,
        "LayoutUncompressed",         // 0x00001000,
        "LayoutSampleRate",           // 0x00002000,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::QueryPipelineStatsFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "QueryPipelineStatsIaVertices",    // 0x1,
        "QueryPipelineStatsIaPrimitives",  // 0x2,
        "QueryPipelineStatsVsInvocations", // 0x4,
        "QueryPipelineStatsGsInvocations", // 0x8,
        "QueryPipelineStatsGsPrimitives",  // 0x10,
        "QueryPipelineStatsCInvocations",  // 0x20,
        "QueryPipelineStatsCPrimitives",   // 0x40,
        "QueryPipelineStatsPsInvocations", // 0x80,
        "QueryPipelineStatsHsInvocations", // 0x100,
        "QueryPipelineStatsDsInvocations", // 0x200,
        "QueryPipelineStatsCsInvocations", // 0x400,
    };

    BeginList(false);

    // Treat the "all stats" flag specially.
    if (flags == QueryPipelineStatsAll)
    {
        Value("QueryPipelineStatsAll");
    }
    else
    {
        constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
        for (uint32 idx = 0; idx < NumFlags; ++idx)
        {
            if ((flags & (1 << idx)) != 0)
            {
                Value(StringTable[idx]);
            }
        }

        // This will trigger if there are flags missing in our table.
        constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
        PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);
    }

    EndList();
}

// =====================================================================================================================
void LogContext::QueryResultFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "QueryResult64Bit",        // 0x1,
        "QueryResultWait",         // 0x2,
        "QueryResultAvailability", // 0x4,
        "QueryResultPartial",      // 0x8,
        "QueryResultAccumulate",   // 0x10,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ClearColorImageFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "ColorClearAutoSync",   // 0x1,
        "ColorClearForceSlow",  // 0x2,
        "ColorClearSkipIfSlow", // 0x4,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ClearDepthStencilFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "DsClearAutoSync", // 0x1,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
void LogContext::ResolveImageFlags(
    uint32 flags)
{
    const char*const StringTable[] =
    {
        "ImageResolveInvertY", // 0x1,
    };

    BeginList(false);

    constexpr uint32 NumFlags = static_cast<uint32>(ArrayLen(StringTable));
    for (uint32 idx = 0; idx < NumFlags; ++idx)
    {
        if ((flags & (1 << idx)) != 0)
        {
            Value(StringTable[idx]);
        }
    }

    // This will trigger if there are flags missing in our table.
    constexpr uint32 UnusedBitMask = ~((1u << NumFlags) - 1u);
    PAL_ASSERT(TestAnyFlagSet(flags, UnusedBitMask) == false);

    EndList();
}

// =====================================================================================================================
const char* LogContext::GetQueueName(
    QueueType value)
{
    const char*const StringTable[] =
    {
        "Universal",   // QueueTypeUniversal
        "Compute",     // QueueTypeCompute
        "Dma",         // QueueTypeDma
        "Timer",       // QueueTypeTimer
    };

    static_assert(ArrayLen(StringTable) == QueueTypeCount,
                  "The GetQueueName string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < QueueTypeCount);

    return StringTable[idx];
}

// =====================================================================================================================
const char* LogContext::GetEngineName(
    EngineType value)
{
    const char*const StringTable[] =
    {
        "Universal",        // EngineTypeUniversal
        "Compute",          // EngineTypeCompute
        "Dma",              // EngineTypeDma
        "Timer",            // EngineTypeTimer

    };

    static_assert(ArrayLen(StringTable) == EngineTypeCount,
                  "The GetQueueName string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < EngineTypeCount);

    return StringTable[idx];
}

// =====================================================================================================================
const char* LogContext::GetVrsCenterRateName(
    VrsCenterRates value)
{
    const char*const StringTable[] =
    {
        "1x1", // _1x1
        "1x2", // _1x2
        "2x1", // _2x1
        "2x2", // _2x2
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(VrsCenterRates::Max),
                  "The GetVrsCenterRateName string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(VrsCenterRates::Max));

    return StringTable[idx];
}

// =====================================================================================================================
const char* LogContext::GetVrsCombinerStageName(
    VrsCombinerStage value)
{
    const char*const StringTable[] =
    {
        "ProvokingVertex", // ProvokingVertex
        "Primitive",       // Primitive
        "Image",           // Image
        "PsIterSamples",   // PsIterSamples
    };

    static_assert(ArrayLen(StringTable) == static_cast<uint32>(VrsCombinerStage::Max),
                  "The GetVrsCombinerStageName string table needs to be updated.");

    const uint32 idx = static_cast<uint32>(value);
    PAL_ASSERT(idx < static_cast<uint32>(VrsCombinerStage::Max));

    return StringTable[idx];
}

} // InterfaceLogger
} // Pal

#endif
