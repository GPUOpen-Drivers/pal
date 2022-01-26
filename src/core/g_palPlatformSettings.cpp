/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!
//
// This code has been generated automatically. Do not hand-modify this code.
//
// When changes are needed, modify the tools generating this module in the tools\generate directory OR the
// appropriate settings_*.json file
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/platform.h"
#include "core/device.h"
#include "core/platformSettingsLoader.h"
#include "core/g_palPlatformSettings.h"
#include "palInlineFuncs.h"
#include "palHashMapImpl.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace DevDriver::SettingsURIService;

namespace Pal
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void PlatformSettingsLoader::SetupDefaults()
{
    // set setting variables to their default values...
#if PAL_ENABLE_PRINTS_ASSERTS
    m_settings.dbgPrintConfig.infoEnabled = false;
    m_settings.dbgPrintConfig.warningEnabled = false;
    m_settings.dbgPrintConfig.errorEnabled = false;
    m_settings.dbgPrintConfig.ScEnabled = false;
    m_settings.dbgPrintConfig.eventPrintEnabled = false;
    m_settings.dbgPrintConfig.eventPrintCbEnabled = false;
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    m_settings.assertsEnabled = false;
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    m_settings.alertsEnabled = false;
#endif

    m_settings.enableEventLogFile = false;
#if   (__unix__)
    memset(m_settings.eventLogDirectory, 0, 512);
    strncpy(m_settings.eventLogDirectory, "amdpal/", 512);
#else
    memset(m_settings.eventLogDirectory, 0, 512);
    strncpy(m_settings.eventLogDirectory, "amdpal/", 512);
#endif
    memset(m_settings.eventLogFilename, 0, 512);
    strncpy(m_settings.eventLogFilename, "PalEventLog.json", 512);

    m_settings.debugOverlayEnabled = false;
    m_settings.debugOverlayConfig.visualConfirmEnabled = true;
    m_settings.debugOverlayConfig.timeGraphEnabled = false;
    m_settings.debugOverlayConfig.overlayLocation = DebugOverlayLowerLeft;
    memset(m_settings.debugOverlayConfig.renderedByString, 0, 61);
    strncpy(m_settings.debugOverlayConfig.renderedByString, "", 61);
    memset(m_settings.debugOverlayConfig.miscellaneousDebugString, 0, 61);
    strncpy(m_settings.debugOverlayConfig.miscellaneousDebugString, "", 61);
    m_settings.debugOverlayConfig.dateTimeEnabled = false;
    m_settings.debugOverlayConfig.printFrameNumber = false;
    m_settings.debugOverlayConfig.useDebugOverlayOnColorSpaceConversionCopy = false;
    m_settings.timeGraphConfig.gridLineColor = RedColor;
    m_settings.timeGraphConfig.cpuLineColor = YellowColor;
    m_settings.timeGraphConfig.gpuLineColor = GreenColor;
    m_settings.overlayBenchmarkConfig.maxBenchmarkTime = 0;
    m_settings.overlayBenchmarkConfig.usageLogEnable = false;
#if   (__unix__)
    memset(m_settings.overlayBenchmarkConfig.usageLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.usageLogDirectory, "amdpal/", 512);
#else
    memset(m_settings.overlayBenchmarkConfig.usageLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.usageLogDirectory, "amdpal/", 512);
#endif
    memset(m_settings.overlayBenchmarkConfig.usageLogFilename, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.usageLogFilename, "PalUsageLog.txt", 512);
    m_settings.overlayBenchmarkConfig.logFrameStats = false;
#if   (__unix__)
    memset(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, "amdpal/", 512);
#else
    memset(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, "amdpal/", 512);
#endif
    m_settings.overlayBenchmarkConfig.maxLoggedFrames = 65536;
    m_settings.overlayMemoryInfoConfig.combineNonLocal = true;
    m_settings.overlayMemoryInfoConfig.reportCmdAllocator = true;
    m_settings.overlayMemoryInfoConfig.reportExternal = true;
    m_settings.overlayMemoryInfoConfig.reportInternal = true;
    m_settings.overlayMemoryInfoConfig.displayPeakMemUsage = false;
    m_settings.gpuProfilerMode = GpuProfilerDisabled;
    m_settings.gpuProfilerTokenAllocatorSize = 64*1024;
#if   (__unix__)
    memset(m_settings.gpuProfilerConfig.logDirectory, 0, 512);
    strncpy(m_settings.gpuProfilerConfig.logDirectory, "amdpal/", 512);
#else
    memset(m_settings.gpuProfilerConfig.logDirectory, 0, 512);
    strncpy(m_settings.gpuProfilerConfig.logDirectory, "amdpal/", 512);
#endif
    memset(m_settings.gpuProfilerConfig.targetApplication, 0, 256);
    strncpy(m_settings.gpuProfilerConfig.targetApplication, "", 256);
    m_settings.gpuProfilerConfig.startFrame = 0;
    m_settings.gpuProfilerConfig.frameCount = 0;
    m_settings.gpuProfilerConfig.recordPipelineStats = false;
    m_settings.gpuProfilerConfig.breakSubmitBatches = false;
    m_settings.gpuProfilerConfig.ignoreNonDrawDispatchCmdBufs = false;
    m_settings.gpuProfilerConfig.useFullPipelineHash = false;
    m_settings.gpuProfilerConfig.traceModeMask = 0x0;
    m_settings.gpuProfilerConfig.granularity = GpuProfilerGranularityDraw;
    memset(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile, 0, 256);
    strncpy(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile, "", 256);
    m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection = false;
    m_settings.gpuProfilerSqttConfig.tokenMask = 0xffff;
    m_settings.gpuProfilerSqttConfig.seMask = 0xf;
    m_settings.gpuProfilerSqttConfig.pipelineHash = 0x0;
    m_settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash = false;
    m_settings.gpuProfilerSqttConfig.tsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.tsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.vsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.vsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.hsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.hsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.dsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.dsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.gsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.gsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.msHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.msHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.psHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.psHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.csHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.csHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.maxDraws = 0x0;
    m_settings.gpuProfilerSqttConfig.addTtvHashes = false;
    m_settings.gpuProfilerSqttConfig.bufferSize = 1048576;
    m_settings.gpuProfilerSqttConfig.stallBehavior = GpuProfilerStallAlways;
    memset(m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile, 0, 256);
    strncpy(m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile, "", 256);
    m_settings.gpuProfilerSpmConfig.spmTraceInterval = 4096;
    m_settings.gpuProfilerSpmConfig.spmBufferSize = 1048576;
    m_settings.gpuProfilerDfSpmConfig.dfSpmTraceInterval = 1280;
    m_settings.gpuProfilerDfSpmConfig.dfSpmBufferSize = 1;
    m_settings.cmdBufferLoggerEnabled = false;
    m_settings.cmdBufferLoggerConfig.cmdBufferLoggerAnnotations = 0x1ff;
    m_settings.cmdBufferLoggerConfig.embedDrawDispatchInfo = CblEmbedDrawDispatchNone;
    m_settings.pm4InstrumentorEnabled = false;
#if   (__unix__)
    memset(m_settings.pm4InstrumentorConfig.logDirectory, 0, 512);
    strncpy(m_settings.pm4InstrumentorConfig.logDirectory, "amdpal/", 512);
#else
    memset(m_settings.pm4InstrumentorConfig.logDirectory, 0, 512);
    strncpy(m_settings.pm4InstrumentorConfig.logDirectory, "amdpal/", 512);
#endif
    memset(m_settings.pm4InstrumentorConfig.filenameSuffix, 0, 512);
    strncpy(m_settings.pm4InstrumentorConfig.filenameSuffix, "pm4-stats.log", 512);
    m_settings.pm4InstrumentorConfig.dumpMode = Pm4InstrumentorDumpQueueDestroy;
    m_settings.pm4InstrumentorConfig.dumpInterval = 5;
    m_settings.interfaceLoggerEnabled = false;
#if   (__unix__)
    memset(m_settings.interfaceLoggerConfig.logDirectory, 0, 512);
    strncpy(m_settings.interfaceLoggerConfig.logDirectory, "amdpal/", 512);
#else
    memset(m_settings.interfaceLoggerConfig.logDirectory, 0, 512);
    strncpy(m_settings.interfaceLoggerConfig.logDirectory, "amdpal/", 512);
#endif
    m_settings.interfaceLoggerConfig.multithreaded = false;
    m_settings.interfaceLoggerConfig.basePreset = 0x7;
    m_settings.interfaceLoggerConfig.elevatedPreset = 0x1f;

    m_settings.gpuDebugEnabled = false;
    m_settings.gpuDebugConfig.submitOnActionCount = 0;
    m_settings.gpuDebugConfig.tokenAllocatorSize = 64*1024;
    m_settings.gpuDebugConfig.waitIdleSleepMs = 2000;
    m_settings.gpuDebugConfig.singleStep = 0x0;
    m_settings.gpuDebugConfig.cacheFlushInvOnAction = 0x0;
    m_settings.gpuDebugConfig.verificationOptions = 0x1;
    m_settings.gpuDebugConfig.surfaceCaptureHash = 0;
    m_settings.gpuDebugConfig.surfaceCaptureDrawStart = 0;
    m_settings.gpuDebugConfig.surfaceCaptureDrawCount = 0;
#if   (__unix__)
    memset(m_settings.gpuDebugConfig.surfaceCaptureLogDirectory, 0, 512);
    strncpy(m_settings.gpuDebugConfig.surfaceCaptureLogDirectory, "amdpal/", 512);
#else
    memset(m_settings.gpuDebugConfig.surfaceCaptureLogDirectory, 0, 512);
    strncpy(m_settings.gpuDebugConfig.surfaceCaptureLogDirectory, "amdpal/", 512);
#endif
    m_settings.numSettings = g_palPlatformNumSettings;
}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void PlatformSettingsLoader::ReadSettings(Pal::Device* pDevice)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    // First setup the debug print and assert settings
    ReadAssertAndPrintSettings(pDevice);
#endif

    // read from the OS adapter for each individual setting
#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_InfoEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.infoEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_WarningEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.warningEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_ErrorEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.errorEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_ScEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.ScEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_EventPrintEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.eventPrintEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pDbgPrintConfig_EventPrintCbEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.dbgPrintConfig.eventPrintCbEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pAssertsEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.assertsEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS
    pDevice->ReadSetting(pAlertsEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.alertsEnabled,
                           InternalSettingScope::PrivatePalKey);
#endif

    pDevice->ReadSetting(pEnableEventLogFileStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableEventLogFile,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_VisualConfirmEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.visualConfirmEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_TimeGraphEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.timeGraphEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_DebugOverlayLocationStr,
                           Util::ValueType::Uint,
                           &m_settings.debugOverlayConfig.overlayLocation,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_RenderedByStringStr,
                           Util::ValueType::Str,
                           &m_settings.debugOverlayConfig.renderedByString,
                           InternalSettingScope::PrivatePalKey,
                           61);

    pDevice->ReadSetting(pDebugOverlayConfig_MiscellaneousDebugStringStr,
                           Util::ValueType::Str,
                           &m_settings.debugOverlayConfig.miscellaneousDebugString,
                           InternalSettingScope::PrivatePalKey,
                           61);

    pDevice->ReadSetting(pDebugOverlayConfig_DateTimeEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.dateTimeEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_PrintFrameNumberStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.printFrameNumber,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pDebugOverlayConfig_UseDebugOverlayOnColorSpaceConversionCopyStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.useDebugOverlayOnColorSpaceConversionCopy,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pTimeGraphConfig_GridLineColorStr,
                           Util::ValueType::Uint,
                           &m_settings.timeGraphConfig.gridLineColor,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pTimeGraphConfig_CpuLineColorStr,
                           Util::ValueType::Uint,
                           &m_settings.timeGraphConfig.cpuLineColor,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pTimeGraphConfig_GpuLineColorStr,
                           Util::ValueType::Uint,
                           &m_settings.timeGraphConfig.gpuLineColor,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_MaxBenchmarkTimeStr,
                           Util::ValueType::Uint,
                           &m_settings.overlayBenchmarkConfig.maxBenchmarkTime,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_UsageLogEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayBenchmarkConfig.usageLogEnable,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_UsageLogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.overlayBenchmarkConfig.usageLogDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_UsageLogFilenameStr,
                           Util::ValueType::Str,
                           &m_settings.overlayBenchmarkConfig.usageLogFilename,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_LogFrameStatsStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayBenchmarkConfig.logFrameStats,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_FrameStatsLogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.overlayBenchmarkConfig.frameStatsLogDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pOverlayBenchmarkConfig_MaxLoggedFramesStr,
                           Util::ValueType::Uint,
                           &m_settings.overlayBenchmarkConfig.maxLoggedFrames,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayMemoryInfoConfig_CombineNonLocalStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayMemoryInfoConfig.combineNonLocal,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayMemoryInfoConfig_ReportCmdAllocatorStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayMemoryInfoConfig.reportCmdAllocator,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayMemoryInfoConfig_ReportExternalStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayMemoryInfoConfig.reportExternal,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayMemoryInfoConfig_ReportInternalStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayMemoryInfoConfig.reportInternal,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pOverlayMemoryInfoConfig_DisplayPeakMemUsageStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayMemoryInfoConfig.displayPeakMemUsage,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerModeStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerMode,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerTokenAllocatorSizeStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerTokenAllocatorSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_LogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.gpuProfilerConfig.logDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pGpuProfilerConfig_TargetApplicationStr,
                           Util::ValueType::Str,
                           &m_settings.gpuProfilerConfig.targetApplication,
                           InternalSettingScope::PrivatePalKey,
                           256);

    pDevice->ReadSetting(pGpuProfilerConfig_StartFrameStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerConfig.startFrame,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_FrameCountStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerConfig.frameCount,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_RecordPipelineStatsStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerConfig.recordPipelineStats,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_BreakSubmitBatchesStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerConfig.breakSubmitBatches,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_IgnoreNonDrawDispatchCmdBufsStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerConfig.ignoreNonDrawDispatchCmdBufs,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_UseFullPipelineHashStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerConfig.useFullPipelineHash,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_TraceModeMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerConfig.traceModeMask,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_GranularityStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerConfig.granularity,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerPerfCounterConfig_GlobalPerfCounterConfigFileStr,
                           Util::ValueType::Str,
                           &m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile,
                           InternalSettingScope::PrivatePalKey,
                           256);

    pDevice->ReadSetting(pGpuProfilerPerfCounterConfig_CacheFlushOnCounterCollectionStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_TokenMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSqttConfig.tokenMask,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_SEMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSqttConfig.seMask,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_PipelineHashStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.pipelineHash,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_PipelineHashAsApiPsoHashStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_TsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.tsHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_TsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.tsHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_VsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.vsHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_VsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.vsHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_HsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.hsHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_HsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.hsHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_DsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.dsHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_DsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.dsHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_GsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.gsHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_GsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.gsHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_MsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.msHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_MsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.msHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_PsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.psHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_PsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.psHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_CsHashHiStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.csHashHi,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_CsHashLoStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.csHashLo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_MaxDrawsStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSqttConfig.maxDraws,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_AddTtvHashesStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuProfilerSqttConfig.addTtvHashes,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_BufferSizeStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSqttConfig.bufferSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSqttConfig_StallBehaviorStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSqttConfig.stallBehavior,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSpmConfig_SpmPerfCounterConfigFileStr,
                           Util::ValueType::Str,
                           &m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile,
                           InternalSettingScope::PrivatePalKey,
                           256);

    pDevice->ReadSetting(pGpuProfilerSpmConfig_SpmTraceIntervalStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSpmConfig.spmTraceInterval,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerSpmConfig_SpmBufferSizeStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerSpmConfig.spmBufferSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerDfSpmConfig_DfSpmTraceIntervalStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerDfSpmConfig.dfSpmTraceInterval,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerDfSpmConfig_DfSpmBufferSizeStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuProfilerDfSpmConfig.dfSpmBufferSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pCmdBufferLoggerEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdBufferLoggerEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pCmdBufferLoggerConfig_CmdBufferLoggerAnnotationsStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufferLoggerConfig.cmdBufferLoggerAnnotations,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pCmdBufferLoggerConfig_EmbedDrawDispatchInfoStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufferLoggerConfig.embedDrawDispatchInfo,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pPm4InstrumentorEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.pm4InstrumentorEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pPm4InstrumentorConfig_LogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.pm4InstrumentorConfig.logDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pPm4InstrumentorConfig_FilenameSuffixStr,
                           Util::ValueType::Str,
                           &m_settings.pm4InstrumentorConfig.filenameSuffix,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pPm4InstrumentorConfig_DumpModeStr,
                           Util::ValueType::Uint,
                           &m_settings.pm4InstrumentorConfig.dumpMode,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pPm4InstrumentorConfig_DumpIntervalStr,
                           Util::ValueType::Uint,
                           &m_settings.pm4InstrumentorConfig.dumpInterval,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pInterfaceLoggerEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.interfaceLoggerEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pInterfaceLoggerConfig_LogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.interfaceLoggerConfig.logDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    pDevice->ReadSetting(pInterfaceLoggerConfig_MultithreadedStr,
                           Util::ValueType::Boolean,
                           &m_settings.interfaceLoggerConfig.multithreaded,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pInterfaceLoggerConfig_BasePresetStr,
                           Util::ValueType::Uint,
                           &m_settings.interfaceLoggerConfig.basePreset,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pInterfaceLoggerConfig_ElevatedPresetStr,
                           Util::ValueType::Uint,
                           &m_settings.interfaceLoggerConfig.elevatedPreset,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.gpuDebugEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SubmitOnActionCountStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.submitOnActionCount,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_TokenAllocatorSizeStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuDebugConfig.tokenAllocatorSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_WaitIdleSleepMsStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.waitIdleSleepMs,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SingleStepStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.singleStep,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_CacheFlushInvOnActionStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.cacheFlushInvOnAction,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_VerificationOptionsStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.verificationOptions,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SurfaceCaptureHashStr,
                           Util::ValueType::Uint64,
                           &m_settings.gpuDebugConfig.surfaceCaptureHash,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SurfaceCaptureDrawStartStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.surfaceCaptureDrawStart,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SurfaceCaptureDrawCountStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuDebugConfig.surfaceCaptureDrawCount,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuDebugConfig_SurfaceCaptureLogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.gpuDebugConfig.surfaceCaptureLogDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

}

// =====================================================================================================================
// Initializes the SettingInfo hash map and array of setting hashes.
void PlatformSettingsLoader::InitSettingsInfo()
{
    SettingInfo info = {};
#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.infoEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.infoEnabled);
    m_settingsInfoMap.Insert(87264462, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.warningEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.warningEnabled);
    m_settingsInfoMap.Insert(3111217572, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.errorEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.errorEnabled);
    m_settingsInfoMap.Insert(1058771018, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.ScEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.ScEnabled);
    m_settingsInfoMap.Insert(2827996440, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.eventPrintEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.eventPrintEnabled);
    m_settingsInfoMap.Insert(4283850211, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.eventPrintCbEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.eventPrintCbEnabled);
    m_settingsInfoMap.Insert(74653004, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.assertsEnabled;
    info.valueSize = sizeof(m_settings.assertsEnabled);
    m_settingsInfoMap.Insert(1110605001, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.alertsEnabled;
    info.valueSize = sizeof(m_settings.alertsEnabled);
    m_settingsInfoMap.Insert(3333004859, info);
#endif

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableEventLogFile;
    info.valueSize = sizeof(m_settings.enableEventLogFile);
    m_settingsInfoMap.Insert(3288205286, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.eventLogDirectory;
    info.valueSize = sizeof(m_settings.eventLogDirectory);
    m_settingsInfoMap.Insert(3789517094, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.eventLogFilename;
    info.valueSize = sizeof(m_settings.eventLogFilename);
    m_settingsInfoMap.Insert(3387502554, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayEnabled);
    m_settingsInfoMap.Insert(3362163801, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.visualConfirmEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.visualConfirmEnabled);
    m_settingsInfoMap.Insert(1802476957, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.timeGraphEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.timeGraphEnabled);
    m_settingsInfoMap.Insert(2933558408, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.debugOverlayConfig.overlayLocation;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.overlayLocation);
    m_settingsInfoMap.Insert(3045745206, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.debugOverlayConfig.renderedByString;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.renderedByString);
    m_settingsInfoMap.Insert(3912270641, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.debugOverlayConfig.miscellaneousDebugString;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.miscellaneousDebugString);
    m_settingsInfoMap.Insert(1196026490, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.dateTimeEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.dateTimeEnabled);
    m_settingsInfoMap.Insert(239137718, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.printFrameNumber;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.printFrameNumber);
    m_settingsInfoMap.Insert(2763643877, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.useDebugOverlayOnColorSpaceConversionCopy;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.useDebugOverlayOnColorSpaceConversionCopy);
    m_settingsInfoMap.Insert(1533629425, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.gridLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.gridLineColor);
    m_settingsInfoMap.Insert(3989097989, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.cpuLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.cpuLineColor);
    m_settingsInfoMap.Insert(689918007, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.gpuLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.gpuLineColor);
    m_settingsInfoMap.Insert(2929386323, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.maxBenchmarkTime;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.maxBenchmarkTime);
    m_settingsInfoMap.Insert(480313510, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogEnable;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogEnable);
    m_settingsInfoMap.Insert(3176801238, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogDirectory;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogDirectory);
    m_settingsInfoMap.Insert(219820144, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogFilename;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogFilename);
    m_settingsInfoMap.Insert(2551463600, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.logFrameStats;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.logFrameStats);
    m_settingsInfoMap.Insert(266798632, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.frameStatsLogDirectory;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory);
    m_settingsInfoMap.Insert(3945706803, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.maxLoggedFrames;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.maxLoggedFrames);
    m_settingsInfoMap.Insert(3387883484, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.combineNonLocal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.combineNonLocal);
    m_settingsInfoMap.Insert(452099995, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportCmdAllocator;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportCmdAllocator);
    m_settingsInfoMap.Insert(2545297707, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportExternal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportExternal);
    m_settingsInfoMap.Insert(1692103889, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportInternal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportInternal);
    m_settingsInfoMap.Insert(1276999751, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.displayPeakMemUsage;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.displayPeakMemUsage);
    m_settingsInfoMap.Insert(2059768529, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerMode;
    info.valueSize = sizeof(m_settings.gpuProfilerMode);
    m_settingsInfoMap.Insert(3490085415, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerTokenAllocatorSize;
    info.valueSize = sizeof(m_settings.gpuProfilerTokenAllocatorSize);
    m_settingsInfoMap.Insert(2716183183, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerConfig.logDirectory;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.logDirectory);
    m_settingsInfoMap.Insert(602986973, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerConfig.targetApplication;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.targetApplication);
    m_settingsInfoMap.Insert(716949517, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.startFrame;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.startFrame);
    m_settingsInfoMap.Insert(17496565, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.frameCount;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.frameCount);
    m_settingsInfoMap.Insert(3630548216, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.recordPipelineStats;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.recordPipelineStats);
    m_settingsInfoMap.Insert(1092484338, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.breakSubmitBatches;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.breakSubmitBatches);
    m_settingsInfoMap.Insert(2743656777, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.ignoreNonDrawDispatchCmdBufs;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.ignoreNonDrawDispatchCmdBufs);
    m_settingsInfoMap.Insert(2163321285, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.useFullPipelineHash;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.useFullPipelineHash);
    m_settingsInfoMap.Insert(3204367348, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.traceModeMask;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.traceModeMask);
    m_settingsInfoMap.Insert(2717664970, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.granularity;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.granularity);
    m_settingsInfoMap.Insert(1675329864, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile;
    info.valueSize = sizeof(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile);
    m_settingsInfoMap.Insert(1666123781, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection;
    info.valueSize = sizeof(m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection);
    m_settingsInfoMap.Insert(3543519762, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.tokenMask;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.tokenMask);
    m_settingsInfoMap.Insert(258959117, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.seMask;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.seMask);
    m_settingsInfoMap.Insert(113814584, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.pipelineHash;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.pipelineHash);
    m_settingsInfoMap.Insert(562315366, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.pipelineHashAsApiPsoHash);
    m_settingsInfoMap.Insert(1180115076, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.tsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.tsHashHi);
    m_settingsInfoMap.Insert(3100319562, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.tsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.tsHashLo);
    m_settingsInfoMap.Insert(3535846108, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.vsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.vsHashHi);
    m_settingsInfoMap.Insert(3546147188, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.vsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.vsHashLo);
    m_settingsInfoMap.Insert(2975119762, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.hsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.hsHashHi);
    m_settingsInfoMap.Insert(3728558198, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.hsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.hsHashLo);
    m_settingsInfoMap.Insert(3225818008, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.dsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.dsHashHi);
    m_settingsInfoMap.Insert(2656705114, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.dsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.dsHashLo);
    m_settingsInfoMap.Insert(2018464044, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.gsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.gsHashHi);
    m_settingsInfoMap.Insert(4196229765, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.gsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.gsHashLo);
    m_settingsInfoMap.Insert(338172111, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.msHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.msHashHi);
    m_settingsInfoMap.Insert(2228026635, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.msHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.msHashLo);
    m_settingsInfoMap.Insert(2329383897, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.psHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.psHashHi);
    m_settingsInfoMap.Insert(1306425790, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.psHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.psHashLo);
    m_settingsInfoMap.Insert(1340672576, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.csHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.csHashHi);
    m_settingsInfoMap.Insert(2590676505, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.csHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.csHashLo);
    m_settingsInfoMap.Insert(3160424003, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.maxDraws;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.maxDraws);
    m_settingsInfoMap.Insert(2938324269, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.addTtvHashes;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.addTtvHashes);
    m_settingsInfoMap.Insert(121855179, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.bufferSize;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.bufferSize);
    m_settingsInfoMap.Insert(3633385103, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.stallBehavior;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.stallBehavior);
    m_settingsInfoMap.Insert(1808881616, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile);
    m_settingsInfoMap.Insert(1162192613, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmTraceInterval;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmTraceInterval);
    m_settingsInfoMap.Insert(3291932008, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmBufferSize;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmBufferSize);
    m_settingsInfoMap.Insert(1857600927, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerDfSpmConfig.dfSpmTraceInterval;
    info.valueSize = sizeof(m_settings.gpuProfilerDfSpmConfig.dfSpmTraceInterval);
    m_settingsInfoMap.Insert(2932969128, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerDfSpmConfig.dfSpmBufferSize;
    info.valueSize = sizeof(m_settings.gpuProfilerDfSpmConfig.dfSpmBufferSize);
    m_settingsInfoMap.Insert(4160531167, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdBufferLoggerEnabled;
    info.valueSize = sizeof(m_settings.cmdBufferLoggerEnabled);
    m_settingsInfoMap.Insert(1206982834, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufferLoggerConfig.cmdBufferLoggerAnnotations;
    info.valueSize = sizeof(m_settings.cmdBufferLoggerConfig.cmdBufferLoggerAnnotations);
    m_settingsInfoMap.Insert(462141291, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufferLoggerConfig.embedDrawDispatchInfo;
    info.valueSize = sizeof(m_settings.cmdBufferLoggerConfig.embedDrawDispatchInfo);
    m_settingsInfoMap.Insert(1801313176, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.pm4InstrumentorEnabled;
    info.valueSize = sizeof(m_settings.pm4InstrumentorEnabled);
    m_settingsInfoMap.Insert(817764955, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.pm4InstrumentorConfig.logDirectory;
    info.valueSize = sizeof(m_settings.pm4InstrumentorConfig.logDirectory);
    m_settingsInfoMap.Insert(2823822363, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.pm4InstrumentorConfig.filenameSuffix;
    info.valueSize = sizeof(m_settings.pm4InstrumentorConfig.filenameSuffix);
    m_settingsInfoMap.Insert(1848754234, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.pm4InstrumentorConfig.dumpMode;
    info.valueSize = sizeof(m_settings.pm4InstrumentorConfig.dumpMode);
    m_settingsInfoMap.Insert(1873500379, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.pm4InstrumentorConfig.dumpInterval;
    info.valueSize = sizeof(m_settings.pm4InstrumentorConfig.dumpInterval);
    m_settingsInfoMap.Insert(1471065745, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.interfaceLoggerEnabled;
    info.valueSize = sizeof(m_settings.interfaceLoggerEnabled);
    m_settingsInfoMap.Insert(2678054117, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.logDirectory;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.logDirectory);
    m_settingsInfoMap.Insert(3997041373, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.multithreaded;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.multithreaded);
    m_settingsInfoMap.Insert(4177532476, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.basePreset;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.basePreset);
    m_settingsInfoMap.Insert(3886684530, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.elevatedPreset;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.elevatedPreset);
    m_settingsInfoMap.Insert(3991423149, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuDebugEnabled;
    info.valueSize = sizeof(m_settings.gpuDebugEnabled);
    m_settingsInfoMap.Insert(3844687577, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.submitOnActionCount;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.submitOnActionCount);
    m_settingsInfoMap.Insert(1833875306, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuDebugConfig.tokenAllocatorSize;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.tokenAllocatorSize);
    m_settingsInfoMap.Insert(673202515, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.waitIdleSleepMs;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.waitIdleSleepMs);
    m_settingsInfoMap.Insert(616327818, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.singleStep;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.singleStep);
    m_settingsInfoMap.Insert(2565248934, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.cacheFlushInvOnAction;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.cacheFlushInvOnAction);
    m_settingsInfoMap.Insert(454658208, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.verificationOptions;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.verificationOptions);
    m_settingsInfoMap.Insert(3198774615, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuDebugConfig.surfaceCaptureHash;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.surfaceCaptureHash);
    m_settingsInfoMap.Insert(2803473291, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.surfaceCaptureDrawStart;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.surfaceCaptureDrawStart);
    m_settingsInfoMap.Insert(2313928635, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuDebugConfig.surfaceCaptureDrawCount;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.surfaceCaptureDrawCount);
    m_settingsInfoMap.Insert(3264482272, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuDebugConfig.surfaceCaptureLogDirectory;
    info.valueSize = sizeof(m_settings.gpuDebugConfig.surfaceCaptureLogDirectory);
    m_settingsInfoMap.Insert(1085905498, info);

}

// =====================================================================================================================
// Registers the core settings with the Developer Driver settings service.
void PlatformSettingsLoader::DevDriverRegister()
{
    PAL_ASSERT(m_pPlatform != nullptr);    auto* pDevDriverServer = m_pPlatform->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            RegisteredComponent component = {};
            strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);
            component.pPrivateData = static_cast<void*>(this);
            component.pSettingsHashes = &g_palPlatformSettingHashList[0];
            component.numSettings = g_palPlatformNumSettings;
            component.pfnGetValue = ISettingsLoader::GetValue;
            component.pfnSetValue = ISettingsLoader::SetValue;
            component.pSettingsData = &g_palPlatformJsonData[0];
            component.settingsDataSize = sizeof(g_palPlatformJsonData);
            component.settingsDataHash = 2780522613;
            component.settingsDataHeader.isEncoded = true;
            component.settingsDataHeader.magicBufferId = 402778310;
            component.settingsDataHeader.magicBufferOffset = 0;

            pSettingsService->RegisterComponent(component);
        }
    }
}

} // Pal
