/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\generate directory OR settings.cfg
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/**
***************************************************************************************************
* @file  g_palSettings.h
* @brief auto-generated file.
*        Contains the definition for the PAL settings struct and enums for initialization.
***************************************************************************************************
*/
#pragma once

#include "pal.h"
#include "palDevice.h"

namespace Pal
{

enum InternalSettingScope : uint32
{
    PrivateDriverKey = 0x0,
    PublicPalKey = 0x1,
    PrivatePalKey = 0x2,
    PrivatePalGfx6Key = 0x3,
    PrivatePalGfx9Key = 0x4,
    PublicCatalystKey = 0x5,

};

enum PrefetchType : uint32
{
    PrefetchCs = 0,
    PrefetchVs = 1,
    PrefetchHs = 2,
    PrefetchDs = 3,
    PrefetchGs = 4,
    PrefetchPs = 5,
    PrefetchCopyShader = 6,
    NumPrefetchTypes = 7,

};

enum Pm4OptEnable : uint32
{
    Pm4OptDefaultEnable = 0,
    Pm4OptForceEnable = 1,
    Pm4OptForceDisable = 2,

};

enum Pm4OptMode : uint32
{
    Pm4OptModeImmediate = 0,
    Pm4OptModeFinalized = 1,

};

enum PipelineOptFlags : uint32
{
    OptNone = 0x0,
    OptTrimUnusedOutputs = 0x1,

};

enum TileSwizzleBits : uint32
{
    TileSwizzleNone = 0x00000000,
    TileSwizzleColor = 0x00000001,
    TileSwizzleDepth = 0x00000002,
    TileSwizzleShaderRes = 0x00000004,
    TileSwizzleAllBits = 0x00000007,

};

enum IfhMode : uint32
{
    IfhModeDisabled = 0,
    IfhModePal = 1,
    IfhModeKmd = 2,

};

enum PipelineLogFlags : uint32
{
    PipelineLogNone = 0,
    PipelineLogInternal = 0x1,
    PipelineLogExternal = 0x2,
    PipelineLogTextFormat = 0x4,
    PipelineLogElfFormat = 0x8,
    PipelineLogAll = 0xF,

};

enum PipelineLogFilters : uint32
{
    PipelineLogFilterNone = 0,
    PipelineLogFilterCs = 0x01,
    PipelineLogFilterNgg = 0x02,
    PipelineLogFilterGs = 0x04,
    PipelineLogFilterTess = 0x08,
    PipelineLogFilterVsPs = 0x10,

};

enum CmdBufForceOneTimeSubmit : uint32
{
    CmdBufForceOneTimeSubmitDefault = 0,
    CmdBufForceOneTimeSubmitOn = 1,
    CmdBufForceOneTimeSubmitOff = 2,

};

enum TossPointMode : uint32
{
    TossPointNone = 0,
    TossPointAfterRaster = 1,
    TossPointWireframe = 2,
    TossPointAfterSetup = 3,
    TossPointDepthClipDisable = 4,
    TossPointAfterPs = 5,
    TossPointSimplePs = 6,

};

enum CmdBufDumpFormat : uint32
{
    CmdBufDumpFormatText = 0,
    CmdBufDumpFormatBinary = 1,
    CmdBufDumpFormatBinaryHeaders = 2,

};

enum CmdBufDumpMode : uint32
{
    CmdBufDumpModeDisabled = 0,
    CmdBufDumpModeRecordTime = 1,
    CmdBufDumpModeSubmitTime = 2,

};

enum DistributionTessMode : uint32
{
    DistributionTessOff = 0,
    DistributionTessDefault = 1,
    DistributionTessPatch = 2,
    DistributionTessDonut = 3,
    DistributionTessTrapezoid = 4,
    DistributionTessTrapezoidOnly = 5,

};

enum ContextRollOptimizationFlags : uint32
{
    OptFlagNone = 0x00000000,
    PadParamCacheSpace = 0x00000001,

};

enum Addr2Disable4kBSwizzle : uint32
{
    Addr2Disable4kBSwizzleDepth = 0x00000001,
    Addr2Disable4kBSwizzleColor1D = 0x00000002,
    Addr2Disable4kBSwizzleColor2D = 0x00000004,
    Addr2Disable4kBSwizzleColor3D = 0x00000008,

};

enum CommandBufferPreemptionFlags : uint32
{
    PreemptionDisabled = 0,
    UniversalEnginePreemption = 1,
    DmaEnginePreemption = 2,

};

enum Addr2PreferredSwizzleTypeSet : uint32
{
    Addr2PreferredDefault = 0x00000000,
    Addr2PreferredSW_Z = 0x00000001,
    Addr2PreferredSW_S = 0x00000002,
    Addr2PreferredSW_D = 0x00000004,
    Addr2PreferredSW_R = 0x00000008,

};

enum VmAlwaysValidEnable : uint32
{
    VmAlwaysValidForceDisable = 0,
    VmAlwaysValidDefaultEnable = 1,
    VmAlwaysValidForceEnable = 2,

};

/// Pal auto-generated settings struct
struct PalSettings
{
    uint32    textureOptLevel;
    uint32    catalystAI;
    bool    forcePreambleCmdStream;
    uint32    maxNumCmdStreamsPerSubmit;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    uint32    borderColorPaletteSizeLimit;
#endif
    bool    requestHighPriorityVmid;
    bool    requestDebugVmid;
    int32    nonlocalDestGraphicsCopyRbs;
    IfhMode    ifh;
    uint32    idleAfterSubmitGpuMask;
    uint32    tossPointMode;
    bool    forceFixedFuncColorResolve;
    gpusize    unboundDescriptorAddress;
    bool    clearAllocatedLfb;
    uint32    addr2Disable4kBSwizzleMode;
    bool    overlayReportHDR;
    uint32    forcedUserDataSpillThreshold;
    uint32    wholePipelineOptimizations;
    bool    forceHeapPerfToFixedValues;
    float    cpuReadPerfForLocal;
    float    cpuWritePerfForLocal;
    float    gpuReadPerfForLocal;
    float    gpuWritePerfForLocal;
    float    gpuReadPerfForInvisible;
    float    gpuWritePerfForInvisible;
    float    cpuWritePerfForGartUswc;
    float    cpuReadPerfForGartUswc;
    float    gpuReadPerfForGartUswc;
    float    gpuWritePerfForGartUswc;
    float    cpuReadPerfForGartCacheable;
    float    cpuWritePerfForGartCacheable;
    float    gpuReadPerfForGartCacheable;
    float    gpuWritePerfForGartCacheable;
    bool    allocationListReusable;
    uint32    fenceTimeoutOverrideInSec;
    bool    updateOneGpuVirtualAddress;
    bool    alwaysResident;
    VmAlwaysValidEnable    enableVmAlwaysValid;
    bool    disableSyncObject;
    bool    disableSyncobjFence;
    CmdBufDumpMode    cmdBufDumpMode;
    CmdBufDumpFormat    cmdBufDumpFormat;
    char    cmdBufDumpDirectory[512];
    uint32    submitTimeCmdBufDumpStartFrame;
    uint32    submitTimeCmdBufDumpEndFrame;
    bool    logCmdBufCommitSizes;
    uint32    logPipelines;
    uint32    filterPipelineLogsByType;
    uint64    logPipelineHash;
    bool    logShadersSeparately;
    bool    logDuplicatePipelines;
    char    pipelineLogDirectory[512];
    bool    embedPipelineDisassembly;
    uint32    cmdStreamReserveLimit;
    bool    cmdStreamEnableMemsetOnReserve;
    uint32    cmdStreamMemsetValue;
    bool    cmdBufChunkEnableStagingBuffer;
    bool    cmdAllocatorFreeOnReset;
    Pm4OptEnable    cmdBufOptimizePm4;
    Pm4OptMode    cmdBufOptimizePm4Mode;
    CmdBufForceOneTimeSubmit    cmdBufForceOneTimeSubmit;
    uint32    commandBufferPreemptionFlags;
    bool    commandBufferForceCeRamDumpInPostamble;
    bool    cmdUtilVerifyShadowedRegRanges;
    uint32    submitOptModeOverride;
    uint32    tileSwizzleMode;
    bool    enableVidMmGpuVaMappingValidation;
    uint32    addr2PreferredSwizzleTypeSet;
    size_t    shaderPrefetchMinSize;
    size_t    shaderPrefetchClampSize;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    bool    allowNonIeeeOperations;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    bool    appendBufPerWaveAtomic;
#endif
    uint32    maxAvailableSgpr;
    uint32    maxAvailableVgpr;
    uint32    maxThreadGroupsPerComputeUnit;
    uint32    ifhGpuMask;
    bool    hwCompositingEnabled;
    bool    mgpuCompatibilityEnabled;
    bool    peerMemoryEnabled;
    bool    forcePresentViaGdi;
    bool    presentViaOglRuntime;
    bool    debugOverlayEnabled;
    bool    visualConfirmEnabled;
    bool    timeGraphEnabled;
    DebugOverlayLocation    debugOverlayLocation;
    TimeGraphColor    timeGraphGridLineColor;
    TimeGraphColor    timeGraphCpuLineColor;
    TimeGraphColor    timeGraphGpuLineColor;
    uint32    maxBenchmarkTime;
    bool    debugUsageLogEnable;
    char    debugUsageLogDirectory[512];
    char    debugUsageLogFilename[512];
    bool    logFrameStats;
    char    frameStatsLogDirectory[512];
    uint32    maxLoggedFrames;
    bool    overlayCombineNonLocal;
    bool    overlayReportCmdAllocator;
    bool    overlayReportExternal;
    bool    overlayReportInternal;
    char    renderedByString[61];
    char    miscellaneousDebugString[61];
    bool    printFrameNumber;
    GpuProfilerMode    gpuProfilerMode;
    uint32    gpuProfilerTraceModeMask;
    char    gpuProfilerLogDirectory[512];
    uint32    gpuProfilerStartFrame;
    uint32    gpuProfilerFrameCount;
    bool    gpuProfilerRecordPipelineStats;
    char    gpuProfilerGlobalPerfCounterConfigFile[256];
    bool    gpuProfilerGlobalPerfCounterPerInstance;
    bool    gpuProfilerBreakSubmitBatches;
    bool    gpuProfilerCacheFlushOnCounterCollection;
    GpuProfilerGranularity    gpuProfilerGranularity;
    uint32    gpuProfilerSqThreadTraceTokenMask;
    uint64    gpuProfilerSqttPipelineHash;
    uint64    gpuProfilerSqttVsHashHi;
    uint64    gpuProfilerSqttVsHashLo;
    uint64    gpuProfilerSqttHsHashHi;
    uint64    gpuProfilerSqttHsHashLo;
    uint64    gpuProfilerSqttDsHashHi;
    uint64    gpuProfilerSqttDsHashLo;
    uint64    gpuProfilerSqttGsHashHi;
    uint64    gpuProfilerSqttGsHashLo;
    uint64    gpuProfilerSqttPsHashHi;
    uint64    gpuProfilerSqttPsHashLo;
    uint64    gpuProfilerSqttCsHashHi;
    uint64    gpuProfilerSqttCsHashLo;
    uint32    gpuProfilerSqttMaxDraws;
    size_t    gpuProfilerSqttBufferSize;
    size_t    gpuProfilerSpmBufferSize;
    uint32    gpuProfilerSpmTraceInterval;
    char    gpuProfilerSpmPerfCounterConfigFile[256];
    bool    cmdBufferLoggerEnabled;
    uint32    cmdBufferLoggerFlags;
    bool    interfaceLoggerEnabled;
    char    interfaceLoggerDirectory[512];
    bool    interfaceLoggerMultithreaded;
    uint32    interfaceLoggerBasePreset;
    uint32    interfaceLoggerElevatedPreset;
};

static const char* pTFQStr = "TFQ";
static const char* pCatalystAIStr = "CatalystAI";
static const char* pForcePreambleCmdStreamStr = "#2987947496";
static const char* pMaxNumCmdStreamsPerSubmitStr = "#2467045849";
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
static const char* pBorderColorPaletteSizeLimitStr = "#161754163";
#endif
static const char* pRequestHighPriorityVmidStr = "#1580739202";
static const char* pRequestDebugVmidStr = "#359792145";
static const char* pNonlocalDestGraphicsCopyRbsStr = "#501901000";
static const char* pIFHStr = "#3299864138";
static const char* pIdleAfterSubmitGpuMaskStr = "#2665794079";
static const char* pTossPointModeStr = "#440136999";
static const char* pForceFixedFuncColorResolveStr = "#4239167273";
static const char* pUnboundDescriptorAddressStr = "#2972919517";
static const char* pClearAllocatedLfbStr = "#2657420565";
static const char* pAddr2Disable4KbSwizzleModeStr = "#2252676842";
static const char* pOverlayReportHDRStr = "#2354711641";
static const char* pForcedUserDataSpillThresholdStr = "#956042398";
static const char* pWholePipelineOptimizationsStr = "#2263765076";
static const char* pForceHeapPerfToFixedValuesStr = "#2415703124";
static const char* pAllocationListReusableStr = "#1727036994";
static const char* pFenceTimeoutOverrideStr = "#970172817";
static const char* pUpdateOneGpuVirtualAddressStr = "#4178383571";
static const char* pAlwaysResidentStr = "#198913068";
static const char* pEnableVmAlwaysValidStr = "#1718264096";
static const char* pDisableSyncObjectStr = "#830933859";
static const char* pDisableSyncobjFenceStr = "#1287715858";
static const char* pCmdBufDumpModeStr = "#3607991033";
static const char* pCmdBufDumpFormatStr = "#1905164977";
static const char* pCmdBufDumpDirectoryStr = "#3293295025";
static const char* pSubmitTimeCmdBufDumpStartFrameStr = "#1639305458";
static const char* pSubmitTimeCmdBufDumpEndFrameStr = "#4221961293";
static const char* pLogCmdBufCommitSizesStr = "#2222002517";
static const char* pLogPipelineInfoStr = "#835791563";
static const char* pFilterPipelineInfoLogsByTypeStr = "#3309548659";
static const char* pLogPipelineHashStr = "#1626813327";
static const char* pLogShadersSeparatelyStr = "#3046934127";
static const char* pLogDuplicatePipelinesStr = "#3439913997";
static const char* pPipelineLogDirectoryStr = "#465486434";
static const char* pEmbedPipelineDisassemblyStr = "#2652059704";
static const char* pCmdStreamReserveLimitStr = "#3843913604";
static const char* pCmdStreamEnableMemsetOnReserveStr = "#3927521274";
static const char* pCmdStreamMemsetValueStr = "#3661455441";
static const char* pCmdBufChunkEnableStagingBufferStr = "#169161685";
static const char* pCmdAllocatorFreeOnResetStr = "#1461164706";
static const char* pCmdBufOptimizePm4Str = "#1018895288";
static const char* pCmdBufOptimizePm4ModeStr = "#2490816619";
static const char* pCmdBufForceOneTimeSubmitStr = "#909934676";
static const char* pCommandBufferPreemptionFlagsStr = "#3169089006";
static const char* pCommandBufferForceCeRamDumpInPostambleStr = "#3413911781";
static const char* pCmdUtilVerifyShadowedRegRangesStr = "#3890704045";
static const char* pSubmitOptModeOverrideStr = "#3054810609";
static const char* pTileSwizzleModeStr = "#1146877010";
static const char* pEnableVidMmGpuVaMappingValidationStr = "#2751785051";
static const char* pAddr2PreferredSwizzleTypeSetStr = "#1836557167";
static const char* pShaderPrefetchMinSizeStr = "#4025766232";
static const char* pShaderPrefetchClampSizeStr = "#2406290039";
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
static const char* pAllowNonIeeeOperationsStr = "#3117105655";
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
static const char* pAppendBufPerWaveAtomicStr = "#976973173";
#endif
static const char* pMaxAvailableSgprStr = "#1008439776";
static const char* pMaxAvailableVgprStr = "#2116546305";
static const char* pMaxThreadGroupsPerComputeUnitStr = "#1284517999";
static const char* pIfhGpuMaskStr = "#3517626664";
static const char* pHwCompositingEnabledStr = "#1872169717";
static const char* pMgpuCompatibilityEnabledStr = "#1177937299";
static const char* pPeerMemoryEnabledStr = "#259362511";
static const char* pForcePresentViaGdiStr = "#2607871653";
static const char* pPresentViaOglRuntimeStr = "#2466363770";
static const char* pDebugOverlayEnabledStr = "#3362163801";
static const char* pVisualConfirmEnabledStr = "#992222976";
static const char* pTimeGraphEnabledStr = "#1298708521";
static const char* pDebugOverlayLocationStr = "#2819484351";
static const char* pTimeGraphGridLineColorStr = "#1434093493";
static const char* pTimeGraphCpuLineColorStr = "#2648094951";
static const char* pTimeGraphGpuLineColorStr = "#1735393667";
static const char* pMaxBenchmarkTimeStr = "#711146025";
static const char* pUsageLogEnableStr = "#4130971273";
static const char* pUsageLogDirectoryStr = "#1158124281";
static const char* pUsageLogFilenameStr = "#3402253111";
static const char* pLogFrameStatsStr = "#108678897";
static const char* pFrameStatsLogDirectoryStr = "#836255284";
static const char* pMaxLoggedFramesStr = "#3935065997";
static const char* pOverlayCombineNonLocalStr = "#994885254";
static const char* pOverlayReportCmdAllocatorStr = "#56171164";
static const char* pOverlayReportExternalStr = "#2790249778";
static const char* pOverlayReportInternalStr = "#1496719512";
static const char* pRenderedByStringStr = "#980561148";
static const char* pMiscellaneousDebugStringStr = "#4023873719";
static const char* pPrintFrameNumberStr = "#954971216";
static const char* pGpuProfilerModeStr = "#3490085415";
static const char* pGpuProfilerTraceModeMaskStr = "#2271979050";
static const char* pGpuProfilerLogDirectoryStr = "#2398691325";
static const char* pGpuProfilerStartFrameStr = "#67505173";
static const char* pGpuProfilerFrameCountStr = "#1057983384";
static const char* pGpuProfilerRecordPipelineStatsStr = "#227092562";
static const char* pGpuProfilerGlobalPerfCounterConfigFileStr = "#2394313408";
static const char* pGpuProfilerGlobalPerfCounterPerInstanceStr = "#3789667260";
static const char* pGpuProfilerBreakSubmitBatchesStr = "#73055401";
static const char* pGpuProfilerCacheFlushOnCounterCollectionStr = "#1108935575";
static const char* pGpuProfilerGranularityStr = "#488491816";
static const char* pGpuProfilerSqThreadTraceTokenMaskStr = "#1470127834";
static const char* pGpuProfilerSqThreadTracePipelineHashStr = "#4079831619";
static const char* pGpuProfilerSqThreadTraceVsHashHiStr = "#3452947205";
static const char* pGpuProfilerSqThreadTraceVsHashLoStr = "#3889856847";
static const char* pGpuProfilerSqThreadTraceHsHashHiStr = "#370224511";
static const char* pGpuProfilerSqThreadTraceHsHashLoStr = "#335977725";
static const char* pGpuProfilerSqThreadTraceDsHashHiStr = "#3300751059";
static const char* pGpuProfilerSqThreadTraceDsHashLoStr = "#3938888961";
static const char* pGpuProfilerSqThreadTraceGsHashHiStr = "#2802864628";
static const char* pGpuProfilerSqThreadTraceGsHashLoStr = "#2231837202";
static const char* pGpuProfilerSqThreadTracePsHashHiStr = "#3602601607";
static const char* pGpuProfilerSqThreadTracePsHashLoStr = "#3031574181";
static const char* pGpuProfilerSqThreadTraceCsHashHiStr = "#2859343864";
static const char* pGpuProfilerSqThreadTraceCsHashLoStr = "#2960701126";
static const char* pGpuProfilerSqThreadTraceMaxDrawsStr = "#2376630876";
static const char* pGpuProfilerSqttBufferSizeStr = "#1929772047";
static const char* pGpuProfilerSpmBufferSizeStr = "#1397779569";
static const char* pGpuProfilerSpmTraceIntervalStr = "#3435305474";
static const char* pGpuProfilerSpmPerfCounterConfigFileStr = "#4239435699";
static const char* pCmdBufferLoggerEnabledStr = "#1206982834";
static const char* pCmdBufferLoggerFlagsStr = "#2297477296";
static const char* pInterfaceLoggerEnabledStr = "#2678054117";
static const char* pInterfaceLoggerDirectoryStr = "#1242063119";
static const char* pInterfaceLoggerMultithreadedStr = "#3296449564";
static const char* pInterfaceLoggerBasePresetStr = "#202615826";
static const char* pInterfaceLoggerElevatedPresetStr = "#156677581";

} // Pal