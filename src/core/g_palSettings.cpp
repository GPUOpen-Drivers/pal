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


#include "core/device.h"

#include "core/settingsLoader.h"

#include "core/g_palSettings.h"

#include "palInlineFuncs.h"

namespace Pal
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::SetupDefaults(
    PalSettings* pSettings)
{
    // set setting variables to their default values...
    pSettings->textureOptLevel = TextureFilterOptimizationsEnabled;
    pSettings->catalystAI = CatalystAiEnable;
    pSettings->forcePreambleCmdStream = true;
    pSettings->maxNumCmdStreamsPerSubmit = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    pSettings->borderColorPaletteSizeLimit = 4096;
#endif
    pSettings->requestHighPriorityVmid = false;
    pSettings->requestDebugVmid = false;
    pSettings->nonlocalDestGraphicsCopyRbs = 0;
    pSettings->ifh = IfhModeDisabled;
    pSettings->idleAfterSubmitGpuMask = 0;
    pSettings->tossPointMode = TossPointNone;
    pSettings->forceFixedFuncColorResolve = false;
    pSettings->unboundDescriptorAddress = 0xDEADBEEFDEADBEEF;
    pSettings->clearAllocatedLfb = false;
    pSettings->addr2Disable4kBSwizzleMode = 0;
    pSettings->overlayReportHDR = true;
    pSettings->forcedUserDataSpillThreshold = 0xFFFF;
    pSettings->wholePipelineOptimizations = OptTrimUnusedOutputs;
    pSettings->forceHeapPerfToFixedValues = false;
    pSettings->cpuReadPerfForLocal = 1.0f;
    pSettings->cpuWritePerfForLocal = 1.0f;
    pSettings->gpuReadPerfForLocal = 1.0f;
    pSettings->gpuWritePerfForLocal = 1.0f;
    pSettings->gpuReadPerfForInvisible = 1.0f;
    pSettings->gpuWritePerfForInvisible = 1.0f;
    pSettings->cpuWritePerfForGartUswc = 1.0f;
    pSettings->cpuReadPerfForGartUswc = 1.0f;
    pSettings->gpuReadPerfForGartUswc = 1.0f;
    pSettings->gpuWritePerfForGartUswc = 1.0f;
    pSettings->cpuReadPerfForGartCacheable = 1.0f;
    pSettings->cpuWritePerfForGartCacheable = 1.0f;
    pSettings->gpuReadPerfForGartCacheable = 1.0f;
    pSettings->gpuWritePerfForGartCacheable = 1.0f;
    pSettings->allocationListReusable = true;
    pSettings->fenceTimeoutOverrideInSec = 0;
    pSettings->updateOneGpuVirtualAddress = false;
    pSettings->alwaysResident = false;
    pSettings->enableVmAlwaysValid = VmAlwaysValidDefaultEnable;
    pSettings->disableSyncObject = false;
    pSettings->disableSyncobjFence = true;
    pSettings->cmdBufDumpMode = CmdBufDumpModeDisabled;
    pSettings->cmdBufDumpFormat = CmdBufDumpFormatText;
    memset(pSettings->cmdBufDumpDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->cmdBufDumpDirectory, "C:\\PalCmdBuffers\\", 512);
#elif (__unix__)
    strncpy(pSettings->cmdBufDumpDirectory, "/tmp/amdpal/", 512);
#endif
    pSettings->submitTimeCmdBufDumpStartFrame = 0;
    pSettings->submitTimeCmdBufDumpEndFrame = 0;
    pSettings->logCmdBufCommitSizes = false;
    pSettings->logPipelines = PipelineLogNone;
    pSettings->filterPipelineLogsByType = PipelineLogFilterNone;
    pSettings->logPipelineHash = 0;
    pSettings->logShadersSeparately = false;
    pSettings->logDuplicatePipelines = true;
    memset(pSettings->pipelineLogDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->pipelineLogDirectory, "C:\\PalPipelines\\", 512);
#elif (__unix__)
    strncpy(pSettings->pipelineLogDirectory, "/tmp/amdpal/", 512);
#endif
    pSettings->embedPipelineDisassembly = false;
    pSettings->cmdStreamReserveLimit = 256;
    pSettings->cmdStreamEnableMemsetOnReserve = false;
    pSettings->cmdStreamMemsetValue = 0xFFFFFFFF;
    pSettings->cmdBufChunkEnableStagingBuffer = false;
    pSettings->cmdAllocatorFreeOnReset = false;
    pSettings->cmdBufOptimizePm4 = Pm4OptDefaultEnable;
    pSettings->cmdBufOptimizePm4Mode = Pm4OptModeImmediate;
    pSettings->cmdBufForceOneTimeSubmit = CmdBufForceOneTimeSubmitDefault;
    pSettings->commandBufferPreemptionFlags = UniversalEnginePreemption | DmaEnginePreemption;
    pSettings->commandBufferForceCeRamDumpInPostamble = false;
    pSettings->cmdUtilVerifyShadowedRegRanges = true;
    pSettings->submitOptModeOverride = 0;
    pSettings->tileSwizzleMode = TileSwizzleAllBits;
    pSettings->enableVidMmGpuVaMappingValidation = false;
    pSettings->addr2PreferredSwizzleTypeSet = Addr2PreferredDefault;
    pSettings->shaderPrefetchMinSize = 0;
    pSettings->shaderPrefetchClampSize = 0;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    pSettings->allowNonIeeeOperations = false;
#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    pSettings->appendBufPerWaveAtomic = false;
#endif
    pSettings->maxAvailableSgpr = 0;
    pSettings->maxAvailableVgpr = 0;
    pSettings->maxThreadGroupsPerComputeUnit = 0;
    pSettings->ifhGpuMask = 0xF;
    pSettings->hwCompositingEnabled = true;
    pSettings->mgpuCompatibilityEnabled = true;
    pSettings->peerMemoryEnabled = true;
    pSettings->forcePresentViaGdi = false;
    pSettings->presentViaOglRuntime = true;
    pSettings->debugOverlayEnabled = false;
    pSettings->visualConfirmEnabled = true;
    pSettings->timeGraphEnabled = false;
    pSettings->debugOverlayLocation = DebugOverlayLowerLeft;
    pSettings->timeGraphGridLineColor = RedColor;
    pSettings->timeGraphCpuLineColor = YellowColor;
    pSettings->timeGraphGpuLineColor = GreenColor;
    pSettings->maxBenchmarkTime = 0;
    pSettings->debugUsageLogEnable = false;
    memset(pSettings->debugUsageLogDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->debugUsageLogDirectory, "C:\\PalLog\\", 512);
#elif (__unix__)
    strncpy(pSettings->debugUsageLogDirectory, "/tmp/amdpal/", 512);
#endif
    memset(pSettings->debugUsageLogFilename, 0, 512);
    strncpy(pSettings->debugUsageLogFilename, "PalUsageLog.txt", 512);
    pSettings->logFrameStats = false;
    memset(pSettings->frameStatsLogDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->frameStatsLogDirectory, "C:\\PalLog\\", 512);
#elif (__unix__)
    strncpy(pSettings->frameStatsLogDirectory, "/tmp/amdpal/", 512);
#endif
    pSettings->maxLoggedFrames = 65536;
    pSettings->overlayCombineNonLocal = true;
    pSettings->overlayReportCmdAllocator = true;
    pSettings->overlayReportExternal = true;
    pSettings->overlayReportInternal = true;
    memset(pSettings->renderedByString, 0, 61);
    strncpy(pSettings->renderedByString, "", 61);
    memset(pSettings->miscellaneousDebugString, 0, 61);
    strncpy(pSettings->miscellaneousDebugString, "", 61);
    pSettings->printFrameNumber = false;
    pSettings->gpuProfilerMode = GpuProfilerDisabled;
    pSettings->gpuProfilerTraceModeMask = 0x0000;
    memset(pSettings->gpuProfilerLogDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->gpuProfilerLogDirectory, "C:\\PalLog\\", 512);
#elif (__unix__)
    strncpy(pSettings->gpuProfilerLogDirectory, "/tmp/amdpal/", 512);
#endif
    pSettings->gpuProfilerStartFrame = 1;
    pSettings->gpuProfilerFrameCount = 0;
    pSettings->gpuProfilerRecordPipelineStats = false;
    memset(pSettings->gpuProfilerGlobalPerfCounterConfigFile, 0, 256);
    strncpy(pSettings->gpuProfilerGlobalPerfCounterConfigFile, "", 256);
    pSettings->gpuProfilerGlobalPerfCounterPerInstance = false;
    pSettings->gpuProfilerBreakSubmitBatches = false;
    pSettings->gpuProfilerCacheFlushOnCounterCollection = false;
    pSettings->gpuProfilerGranularity = GpuProfilerGranularityDraw;
    pSettings->gpuProfilerSqThreadTraceTokenMask = 0xFFFF;
    pSettings->gpuProfilerSqttPipelineHash = 0;
    pSettings->gpuProfilerSqttVsHashHi = 0;
    pSettings->gpuProfilerSqttVsHashLo = 0;
    pSettings->gpuProfilerSqttHsHashHi = 0;
    pSettings->gpuProfilerSqttHsHashLo = 0;
    pSettings->gpuProfilerSqttDsHashHi = 0;
    pSettings->gpuProfilerSqttDsHashLo = 0;
    pSettings->gpuProfilerSqttGsHashHi = 0;
    pSettings->gpuProfilerSqttGsHashLo = 0;
    pSettings->gpuProfilerSqttPsHashHi = 0;
    pSettings->gpuProfilerSqttPsHashLo = 0;
    pSettings->gpuProfilerSqttCsHashHi = 0;
    pSettings->gpuProfilerSqttCsHashLo = 0;
    pSettings->gpuProfilerSqttMaxDraws = 0;
    pSettings->gpuProfilerSqttBufferSize = 1048576;
    pSettings->gpuProfilerSpmBufferSize = 1048576;
    pSettings->gpuProfilerSpmTraceInterval = 4096;
    memset(pSettings->gpuProfilerSpmPerfCounterConfigFile, 0, 256);
    strncpy(pSettings->gpuProfilerSpmPerfCounterConfigFile, "", 256);
    pSettings->cmdBufferLoggerEnabled = false;
    pSettings->cmdBufferLoggerFlags = 0x000001FF;
    pSettings->interfaceLoggerEnabled = false;
    memset(pSettings->interfaceLoggerDirectory, 0, 512);
#if defined(_WIN32)
    strncpy(pSettings->interfaceLoggerDirectory, "C:\\PalLog\\", 512);
#elif (__unix__)
    strncpy(pSettings->interfaceLoggerDirectory, "~/.amdpal/", 512);
#endif
    pSettings->interfaceLoggerMultithreaded = false;
    pSettings->interfaceLoggerBasePreset = 0x7;
    pSettings->interfaceLoggerElevatedPreset = 0x1F;

}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::ReadSettings(
    PalSettings* pSettings)
{
    // read from the OS adapter for each individual setting
    m_pDevice->ReadSetting(pTFQStr,
                             Util::ValueType::Uint,
                             &pSettings->textureOptLevel,
                             InternalSettingScope::PublicCatalystKey);

    m_pDevice->ReadSetting(pCatalystAIStr,
                             Util::ValueType::Uint,
                             &pSettings->catalystAI,
                             InternalSettingScope::PublicCatalystKey);

    m_pDevice->ReadSetting(pForcePreambleCmdStreamStr,
                             Util::ValueType::Boolean,
                             &pSettings->forcePreambleCmdStream,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pMaxNumCmdStreamsPerSubmitStr,
                             Util::ValueType::Uint,
                             &pSettings->maxNumCmdStreamsPerSubmit,
                             InternalSettingScope::PrivatePalKey);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    m_pDevice->ReadSetting(pBorderColorPaletteSizeLimitStr,
                             Util::ValueType::Uint,
                             &pSettings->borderColorPaletteSizeLimit,
                             InternalSettingScope::PrivatePalKey);

#endif
    m_pDevice->ReadSetting(pRequestHighPriorityVmidStr,
                             Util::ValueType::Boolean,
                             &pSettings->requestHighPriorityVmid,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pRequestDebugVmidStr,
                             Util::ValueType::Boolean,
                             &pSettings->requestDebugVmid,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pNonlocalDestGraphicsCopyRbsStr,
                             Util::ValueType::Int,
                             &pSettings->nonlocalDestGraphicsCopyRbs,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pIFHStr,
                             Util::ValueType::Int,
                             &pSettings->ifh,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pIdleAfterSubmitGpuMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->idleAfterSubmitGpuMask,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTossPointModeStr,
                             Util::ValueType::Uint,
                             &pSettings->tossPointMode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pForceFixedFuncColorResolveStr,
                             Util::ValueType::Boolean,
                             &pSettings->forceFixedFuncColorResolve,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pUnboundDescriptorAddressStr,
                             Util::ValueType::Uint64,
                             &pSettings->unboundDescriptorAddress,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pClearAllocatedLfbStr,
                             Util::ValueType::Boolean,
                             &pSettings->clearAllocatedLfb,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pAddr2Disable4KbSwizzleModeStr,
                             Util::ValueType::Uint,
                             &pSettings->addr2Disable4kBSwizzleMode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pOverlayReportHDRStr,
                             Util::ValueType::Boolean,
                             &pSettings->overlayReportHDR,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pForcedUserDataSpillThresholdStr,
                             Util::ValueType::Uint,
                             &pSettings->forcedUserDataSpillThreshold,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pWholePipelineOptimizationsStr,
                             Util::ValueType::Uint,
                             &pSettings->wholePipelineOptimizations,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pForceHeapPerfToFixedValuesStr,
                             Util::ValueType::Boolean,
                             &pSettings->forceHeapPerfToFixedValues,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pAllocationListReusableStr,
                             Util::ValueType::Boolean,
                             &pSettings->allocationListReusable,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pFenceTimeoutOverrideStr,
                             Util::ValueType::Uint,
                             &pSettings->fenceTimeoutOverrideInSec,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pUpdateOneGpuVirtualAddressStr,
                             Util::ValueType::Boolean,
                             &pSettings->updateOneGpuVirtualAddress,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pAlwaysResidentStr,
                             Util::ValueType::Boolean,
                             &pSettings->alwaysResident,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pEnableVmAlwaysValidStr,
                             Util::ValueType::Uint,
                             &pSettings->enableVmAlwaysValid,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pDisableSyncObjectStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableSyncObject,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pDisableSyncobjFenceStr,
                             Util::ValueType::Boolean,
                             &pSettings->disableSyncobjFence,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufDumpModeStr,
                             Util::ValueType::Int,
                             &pSettings->cmdBufDumpMode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufDumpFormatStr,
                             Util::ValueType::Int,
                             &pSettings->cmdBufDumpFormat,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufDumpDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->cmdBufDumpDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pSubmitTimeCmdBufDumpStartFrameStr,
                             Util::ValueType::Int,
                             &pSettings->submitTimeCmdBufDumpStartFrame,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pSubmitTimeCmdBufDumpEndFrameStr,
                             Util::ValueType::Int,
                             &pSettings->submitTimeCmdBufDumpEndFrame,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pLogCmdBufCommitSizesStr,
                             Util::ValueType::Boolean,
                             &pSettings->logCmdBufCommitSizes,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pLogPipelineInfoStr,
                             Util::ValueType::Uint,
                             &pSettings->logPipelines,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pFilterPipelineInfoLogsByTypeStr,
                             Util::ValueType::Uint,
                             &pSettings->filterPipelineLogsByType,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pLogPipelineHashStr,
                             Util::ValueType::Uint64,
                             &pSettings->logPipelineHash,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pLogShadersSeparatelyStr,
                             Util::ValueType::Boolean,
                             &pSettings->logShadersSeparately,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pLogDuplicatePipelinesStr,
                             Util::ValueType::Boolean,
                             &pSettings->logDuplicatePipelines,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pPipelineLogDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->pipelineLogDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pEmbedPipelineDisassemblyStr,
                             Util::ValueType::Boolean,
                             &pSettings->embedPipelineDisassembly,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdStreamReserveLimitStr,
                             Util::ValueType::Uint,
                             &pSettings->cmdStreamReserveLimit,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdStreamEnableMemsetOnReserveStr,
                             Util::ValueType::Boolean,
                             &pSettings->cmdStreamEnableMemsetOnReserve,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdStreamMemsetValueStr,
                             Util::ValueType::Uint,
                             &pSettings->cmdStreamMemsetValue,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufChunkEnableStagingBufferStr,
                             Util::ValueType::Boolean,
                             &pSettings->cmdBufChunkEnableStagingBuffer,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdAllocatorFreeOnResetStr,
                             Util::ValueType::Boolean,
                             &pSettings->cmdAllocatorFreeOnReset,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufOptimizePm4Str,
                             Util::ValueType::Uint,
                             &pSettings->cmdBufOptimizePm4,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufOptimizePm4ModeStr,
                             Util::ValueType::Uint,
                             &pSettings->cmdBufOptimizePm4Mode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufForceOneTimeSubmitStr,
                             Util::ValueType::Uint,
                             &pSettings->cmdBufForceOneTimeSubmit,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCommandBufferPreemptionFlagsStr,
                             Util::ValueType::Uint,
                             &pSettings->commandBufferPreemptionFlags,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCommandBufferForceCeRamDumpInPostambleStr,
                             Util::ValueType::Boolean,
                             &pSettings->commandBufferForceCeRamDumpInPostamble,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdUtilVerifyShadowedRegRangesStr,
                             Util::ValueType::Boolean,
                             &pSettings->cmdUtilVerifyShadowedRegRanges,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pSubmitOptModeOverrideStr,
                             Util::ValueType::Uint,
                             &pSettings->submitOptModeOverride,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTileSwizzleModeStr,
                             Util::ValueType::Uint,
                             &pSettings->tileSwizzleMode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pEnableVidMmGpuVaMappingValidationStr,
                             Util::ValueType::Boolean,
                             &pSettings->enableVidMmGpuVaMappingValidation,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pAddr2PreferredSwizzleTypeSetStr,
                             Util::ValueType::Uint,
                             &pSettings->addr2PreferredSwizzleTypeSet,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pShaderPrefetchMinSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->shaderPrefetchMinSize,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pShaderPrefetchClampSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->shaderPrefetchClampSize,
                             InternalSettingScope::PrivatePalKey);

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    m_pDevice->ReadSetting(pAllowNonIeeeOperationsStr,
                             Util::ValueType::Boolean,
                             &pSettings->allowNonIeeeOperations,
                             InternalSettingScope::PrivatePalKey);

#endif
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION <= 354
    m_pDevice->ReadSetting(pAppendBufPerWaveAtomicStr,
                             Util::ValueType::Boolean,
                             &pSettings->appendBufPerWaveAtomic,
                             InternalSettingScope::PrivatePalKey);

#endif
    m_pDevice->ReadSetting(pMaxAvailableSgprStr,
                             Util::ValueType::Uint,
                             &pSettings->maxAvailableSgpr,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pMaxAvailableVgprStr,
                             Util::ValueType::Uint,
                             &pSettings->maxAvailableVgpr,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pMaxThreadGroupsPerComputeUnitStr,
                             Util::ValueType::Uint,
                             &pSettings->maxThreadGroupsPerComputeUnit,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pIfhGpuMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->ifhGpuMask,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pHwCompositingEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->hwCompositingEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pMgpuCompatibilityEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->mgpuCompatibilityEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pPeerMemoryEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->peerMemoryEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pForcePresentViaGdiStr,
                             Util::ValueType::Boolean,
                             &pSettings->forcePresentViaGdi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pPresentViaOglRuntimeStr,
                             Util::ValueType::Boolean,
                             &pSettings->presentViaOglRuntime,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pDebugOverlayEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->debugOverlayEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pVisualConfirmEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->visualConfirmEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTimeGraphEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->timeGraphEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pDebugOverlayLocationStr,
                             Util::ValueType::Uint,
                             &pSettings->debugOverlayLocation,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTimeGraphGridLineColorStr,
                             Util::ValueType::Uint,
                             &pSettings->timeGraphGridLineColor,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTimeGraphCpuLineColorStr,
                             Util::ValueType::Uint,
                             &pSettings->timeGraphCpuLineColor,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pTimeGraphGpuLineColorStr,
                             Util::ValueType::Uint,
                             &pSettings->timeGraphGpuLineColor,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pMaxBenchmarkTimeStr,
                             Util::ValueType::Uint,
                             &pSettings->maxBenchmarkTime,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pUsageLogEnableStr,
                             Util::ValueType::Boolean,
                             &pSettings->debugUsageLogEnable,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pUsageLogDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->debugUsageLogDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pUsageLogFilenameStr,
                             Util::ValueType::Str,
                             &pSettings->debugUsageLogFilename,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pLogFrameStatsStr,
                             Util::ValueType::Boolean,
                             &pSettings->logFrameStats,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pFrameStatsLogDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->frameStatsLogDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pMaxLoggedFramesStr,
                             Util::ValueType::Uint,
                             &pSettings->maxLoggedFrames,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pOverlayCombineNonLocalStr,
                             Util::ValueType::Boolean,
                             &pSettings->overlayCombineNonLocal,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pOverlayReportCmdAllocatorStr,
                             Util::ValueType::Boolean,
                             &pSettings->overlayReportCmdAllocator,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pOverlayReportExternalStr,
                             Util::ValueType::Boolean,
                             &pSettings->overlayReportExternal,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pOverlayReportInternalStr,
                             Util::ValueType::Boolean,
                             &pSettings->overlayReportInternal,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pRenderedByStringStr,
                             Util::ValueType::Str,
                             &pSettings->renderedByString,
                             InternalSettingScope::PrivatePalKey,
                             61);

    m_pDevice->ReadSetting(pMiscellaneousDebugStringStr,
                             Util::ValueType::Str,
                             &pSettings->miscellaneousDebugString,
                             InternalSettingScope::PrivatePalKey,
                             61);

    m_pDevice->ReadSetting(pPrintFrameNumberStr,
                             Util::ValueType::Boolean,
                             &pSettings->printFrameNumber,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerModeStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerMode,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerTraceModeMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerTraceModeMask,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerLogDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->gpuProfilerLogDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pGpuProfilerStartFrameStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerStartFrame,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerFrameCountStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerFrameCount,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerRecordPipelineStatsStr,
                             Util::ValueType::Boolean,
                             &pSettings->gpuProfilerRecordPipelineStats,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerGlobalPerfCounterConfigFileStr,
                             Util::ValueType::Str,
                             &pSettings->gpuProfilerGlobalPerfCounterConfigFile,
                             InternalSettingScope::PrivatePalKey,
                             256);

    m_pDevice->ReadSetting(pGpuProfilerGlobalPerfCounterPerInstanceStr,
                             Util::ValueType::Boolean,
                             &pSettings->gpuProfilerGlobalPerfCounterPerInstance,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerBreakSubmitBatchesStr,
                             Util::ValueType::Boolean,
                             &pSettings->gpuProfilerBreakSubmitBatches,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerCacheFlushOnCounterCollectionStr,
                             Util::ValueType::Boolean,
                             &pSettings->gpuProfilerCacheFlushOnCounterCollection,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerGranularityStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerGranularity,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceTokenMaskStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerSqThreadTraceTokenMask,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTracePipelineHashStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttPipelineHash,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceVsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttVsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceVsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttVsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceHsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttHsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceHsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttHsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceDsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttDsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceDsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttDsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceGsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttGsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceGsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttGsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTracePsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttPsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTracePsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttPsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceCsHashHiStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttCsHashHi,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceCsHashLoStr,
                             Util::ValueType::Uint64,
                             &pSettings->gpuProfilerSqttCsHashLo,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqThreadTraceMaxDrawsStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerSqttMaxDraws,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSqttBufferSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerSqttBufferSize,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSpmBufferSizeStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerSpmBufferSize,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSpmTraceIntervalStr,
                             Util::ValueType::Uint,
                             &pSettings->gpuProfilerSpmTraceInterval,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pGpuProfilerSpmPerfCounterConfigFileStr,
                             Util::ValueType::Str,
                             &pSettings->gpuProfilerSpmPerfCounterConfigFile,
                             InternalSettingScope::PrivatePalKey,
                             256);

    m_pDevice->ReadSetting(pCmdBufferLoggerEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->cmdBufferLoggerEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pCmdBufferLoggerFlagsStr,
                             Util::ValueType::Uint,
                             &pSettings->cmdBufferLoggerFlags,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pInterfaceLoggerEnabledStr,
                             Util::ValueType::Boolean,
                             &pSettings->interfaceLoggerEnabled,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pInterfaceLoggerDirectoryStr,
                             Util::ValueType::Str,
                             &pSettings->interfaceLoggerDirectory,
                             InternalSettingScope::PrivatePalKey,
                             512);

    m_pDevice->ReadSetting(pInterfaceLoggerMultithreadedStr,
                             Util::ValueType::Boolean,
                             &pSettings->interfaceLoggerMultithreaded,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pInterfaceLoggerBasePresetStr,
                             Util::ValueType::Uint,
                             &pSettings->interfaceLoggerBasePreset,
                             InternalSettingScope::PrivatePalKey);

    m_pDevice->ReadSetting(pInterfaceLoggerElevatedPresetStr,
                             Util::ValueType::Uint,
                             &pSettings->interfaceLoggerElevatedPreset,
                             InternalSettingScope::PrivatePalKey);


}

} // Pal