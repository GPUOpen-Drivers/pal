/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

    m_settings.debugOverlayEnabled = false;
    m_settings.debugOverlayConfig.visualConfirmEnabled = true;
    m_settings.debugOverlayConfig.timeGraphEnabled = false;
    m_settings.debugOverlayConfig.overlayLocation = DebugOverlayLowerLeft;
    memset(m_settings.debugOverlayConfig.renderedByString, 0, 61);
    strncpy(m_settings.debugOverlayConfig.renderedByString, "", 61);
    memset(m_settings.debugOverlayConfig.miscellaneousDebugString, 0, 61);
    strncpy(m_settings.debugOverlayConfig.miscellaneousDebugString, "", 61);
    m_settings.debugOverlayConfig.printFrameNumber = false;
    m_settings.timeGraphConfig.gridLineColor = RedColor;
    m_settings.timeGraphConfig.cpuLineColor = YellowColor;
    m_settings.timeGraphConfig.gpuLineColor = GreenColor;
    m_settings.overlayBenchmarkConfig.maxBenchmarkTime = 0;
    m_settings.overlayBenchmarkConfig.usageLogEnable = false;
    memset(m_settings.overlayBenchmarkConfig.usageLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.usageLogDirectory, "amdpal/", 512);
    memset(m_settings.overlayBenchmarkConfig.usageLogFilename, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.usageLogFilename, "PalUsageLog.txt", 512);
    m_settings.overlayBenchmarkConfig.logFrameStats = false;
    memset(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, 0, 512);
    strncpy(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory, "amdpal/", 512);
    m_settings.overlayBenchmarkConfig.maxLoggedFrames = 65536;
    m_settings.overlayMemoryInfoConfig.combineNonLocal = true;
    m_settings.overlayMemoryInfoConfig.reportCmdAllocator = true;
    m_settings.overlayMemoryInfoConfig.reportExternal = true;
    m_settings.overlayMemoryInfoConfig.reportInternal = true;
    m_settings.gpuProfilerMode = GpuProfilerDisabled;
    memset(m_settings.gpuProfilerConfig.logDirectory, 0, 512);
    strncpy(m_settings.gpuProfilerConfig.logDirectory, "amdpal/", 512);
    m_settings.gpuProfilerConfig.startFrame = 0;
    m_settings.gpuProfilerConfig.frameCount = 0;
    m_settings.gpuProfilerConfig.recordPipelineStats = false;
    m_settings.gpuProfilerConfig.breakSubmitBatches = false;
    m_settings.gpuProfilerConfig.traceModeMask = 0x0;
    memset(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile, 0, 256);
    strncpy(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile, "", 256);
    m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection = false;
    m_settings.gpuProfilerPerfCounterConfig.granularity = GpuProfilerGranularityDraw;
    m_settings.gpuProfilerSqttConfig.tokenMask = 0xffff;
    m_settings.gpuProfilerSqttConfig.seMask = 0xf;
    m_settings.gpuProfilerSqttConfig.pipelineHash = 0x0;
    m_settings.gpuProfilerSqttConfig.vsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.vsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.hsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.hsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.dsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.dsHashLo = 0x0;
    m_settings.gpuProfilerSqttConfig.gsHashHi = 0x0;
    m_settings.gpuProfilerSqttConfig.gsHashLo = 0x0;
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
    m_settings.cmdBufferLoggerEnabled = false;
    m_settings.cmdBufferLoggerFlags = 0x1ff;
    m_settings.interfaceLoggerEnabled = false;
    memset(m_settings.interfaceLoggerConfig.logDirectory, 0, 512);
    strncpy(m_settings.interfaceLoggerConfig.logDirectory, "amdpal/", 512);
    m_settings.interfaceLoggerConfig.multithreaded = false;
    m_settings.interfaceLoggerConfig.basePreset = 0x7;
    m_settings.interfaceLoggerConfig.elevatedPreset = 0x1f;

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

    pDevice->ReadSetting(pDebugOverlayConfig_PrintFrameNumberStr,
                           Util::ValueType::Boolean,
                           &m_settings.debugOverlayConfig.printFrameNumber,
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

    pDevice->ReadSetting(pGpuProfilerModeStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerMode,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pGpuProfilerConfig_LogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.gpuProfilerConfig.logDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

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

    pDevice->ReadSetting(pGpuProfilerConfig_TraceModeMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerConfig.traceModeMask,
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

    pDevice->ReadSetting(pGpuProfilerPerfCounterConfig_GranularityStr,
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerPerfCounterConfig.granularity,
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
                           Util::ValueType::Uint,
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
                           Util::ValueType::Uint,
                           &m_settings.gpuProfilerSpmConfig.spmBufferSize,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pCmdBufferLoggerEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdBufferLoggerEnabled,
                           InternalSettingScope::PrivatePalKey);

    pDevice->ReadSetting(pCmdBufferLoggerFlagsStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufferLoggerFlags,
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
    m_settingsInfoMap.Insert(3336086055, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.warningEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.warningEnabled);
    m_settingsInfoMap.Insert(3827375483, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.errorEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.errorEnabled);
    m_settingsInfoMap.Insert(1444311189, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.ScEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.ScEnabled);
    m_settingsInfoMap.Insert(695309361, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.eventPrintEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.eventPrintEnabled);
    m_settingsInfoMap.Insert(721345714, info);
#endif

#if PAL_ENABLE_PRINTS_ASSERTS

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dbgPrintConfig.eventPrintCbEnabled;
    info.valueSize = sizeof(m_settings.dbgPrintConfig.eventPrintCbEnabled);
    m_settingsInfoMap.Insert(4220374213, info);
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
    info.pValuePtr = &m_settings.debugOverlayEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayEnabled);
    m_settingsInfoMap.Insert(3362163801, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.visualConfirmEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.visualConfirmEnabled);
    m_settingsInfoMap.Insert(1116165338, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.timeGraphEnabled;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.timeGraphEnabled);
    m_settingsInfoMap.Insert(2485887783, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.debugOverlayConfig.overlayLocation;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.overlayLocation);
    m_settingsInfoMap.Insert(248899441, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.debugOverlayConfig.renderedByString;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.renderedByString);
    m_settingsInfoMap.Insert(3965817458, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.debugOverlayConfig.miscellaneousDebugString;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.miscellaneousDebugString);
    m_settingsInfoMap.Insert(1845251913, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.debugOverlayConfig.printFrameNumber;
    info.valueSize = sizeof(m_settings.debugOverlayConfig.printFrameNumber);
    m_settingsInfoMap.Insert(3798504442, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.gridLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.gridLineColor);
    m_settingsInfoMap.Insert(2774338144, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.cpuLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.cpuLineColor);
    m_settingsInfoMap.Insert(3593358936, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.timeGraphConfig.gpuLineColor;
    info.valueSize = sizeof(m_settings.timeGraphConfig.gpuLineColor);
    m_settingsInfoMap.Insert(683848132, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.maxBenchmarkTime;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.maxBenchmarkTime);
    m_settingsInfoMap.Insert(1613865845, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogEnable;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogEnable);
    m_settingsInfoMap.Insert(1439000029, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogDirectory;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogDirectory);
    m_settingsInfoMap.Insert(3345515589, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.usageLogFilename;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.usageLogFilename);
    m_settingsInfoMap.Insert(570887899, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.logFrameStats;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.logFrameStats);
    m_settingsInfoMap.Insert(2989332685, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.frameStatsLogDirectory;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.frameStatsLogDirectory);
    m_settingsInfoMap.Insert(1128423400, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.overlayBenchmarkConfig.maxLoggedFrames;
    info.valueSize = sizeof(m_settings.overlayBenchmarkConfig.maxLoggedFrames);
    m_settingsInfoMap.Insert(1247452473, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.combineNonLocal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.combineNonLocal);
    m_settingsInfoMap.Insert(1412889158, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportCmdAllocator;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportCmdAllocator);
    m_settingsInfoMap.Insert(819062876, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportExternal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportExternal);
    m_settingsInfoMap.Insert(1211449330, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayMemoryInfoConfig.reportInternal;
    info.valueSize = sizeof(m_settings.overlayMemoryInfoConfig.reportInternal);
    m_settingsInfoMap.Insert(373898840, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerMode;
    info.valueSize = sizeof(m_settings.gpuProfilerMode);
    m_settingsInfoMap.Insert(3490085415, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerConfig.logDirectory;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.logDirectory);
    m_settingsInfoMap.Insert(1786197374, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.startFrame;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.startFrame);
    m_settingsInfoMap.Insert(3281941262, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.frameCount;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.frameCount);
    m_settingsInfoMap.Insert(3899735123, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.recordPipelineStats;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.recordPipelineStats);
    m_settingsInfoMap.Insert(3225763835, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerConfig.breakSubmitBatches;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.breakSubmitBatches);
    m_settingsInfoMap.Insert(3699637222, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerConfig.traceModeMask;
    info.valueSize = sizeof(m_settings.gpuProfilerConfig.traceModeMask);
    m_settingsInfoMap.Insert(2733188403, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile;
    info.valueSize = sizeof(m_settings.gpuProfilerPerfCounterConfig.globalPerfCounterConfigFile);
    m_settingsInfoMap.Insert(2182449032, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection;
    info.valueSize = sizeof(m_settings.gpuProfilerPerfCounterConfig.cacheFlushOnCounterCollection);
    m_settingsInfoMap.Insert(1201772335, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerPerfCounterConfig.granularity;
    info.valueSize = sizeof(m_settings.gpuProfilerPerfCounterConfig.granularity);
    m_settingsInfoMap.Insert(3414628368, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.tokenMask;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.tokenMask);
    m_settingsInfoMap.Insert(1136095484, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.seMask;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.seMask);
    m_settingsInfoMap.Insert(4066555951, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.pipelineHash;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.pipelineHash);
    m_settingsInfoMap.Insert(3932789981, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.vsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.vsHashHi);
    m_settingsInfoMap.Insert(3163259987, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.vsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.vsHashLo);
    m_settingsInfoMap.Insert(3801397889, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.hsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.hsHashHi);
    m_settingsInfoMap.Insert(1390489601, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.hsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.hsHashLo);
    m_settingsInfoMap.Insert(886572651, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.dsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.dsHashHi);
    m_settingsInfoMap.Insert(3315456133, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.dsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.dsHashLo);
    m_settingsInfoMap.Insert(3752365775, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.gsHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.gsHashHi);
    m_settingsInfoMap.Insert(1775931482, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.gsHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.gsHashLo);
    m_settingsInfoMap.Insert(1137690412, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.psHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.psHashHi);
    m_settingsInfoMap.Insert(1805758793, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.psHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.psHashLo);
    m_settingsInfoMap.Insert(1301841843, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.csHashHi;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.csHashHi);
    m_settingsInfoMap.Insert(1629297294, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.csHashLo;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.csHashLo);
    m_settingsInfoMap.Insert(1663440912, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.maxDraws;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.maxDraws);
    m_settingsInfoMap.Insert(3208735818, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.addTtvHashes;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.addTtvHashes);
    m_settingsInfoMap.Insert(2774444984, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.bufferSize;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.bufferSize);
    m_settingsInfoMap.Insert(1277078724, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSqttConfig.stallBehavior;
    info.valueSize = sizeof(m_settings.gpuProfilerSqttConfig.stallBehavior);
    m_settingsInfoMap.Insert(1063331229, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmPerfCounterConfigFile);
    m_settingsInfoMap.Insert(1274479618, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmTraceInterval;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmTraceInterval);
    m_settingsInfoMap.Insert(3756226799, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.gpuProfilerSpmConfig.spmBufferSize;
    info.valueSize = sizeof(m_settings.gpuProfilerSpmConfig.spmBufferSize);
    m_settingsInfoMap.Insert(3798430118, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdBufferLoggerEnabled;
    info.valueSize = sizeof(m_settings.cmdBufferLoggerEnabled);
    m_settingsInfoMap.Insert(1206982834, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufferLoggerFlags;
    info.valueSize = sizeof(m_settings.cmdBufferLoggerFlags);
    m_settingsInfoMap.Insert(2297477296, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.interfaceLoggerEnabled;
    info.valueSize = sizeof(m_settings.interfaceLoggerEnabled);
    m_settingsInfoMap.Insert(2678054117, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.logDirectory;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.logDirectory);
    m_settingsInfoMap.Insert(885284478, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.multithreaded;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.multithreaded);
    m_settingsInfoMap.Insert(800910225, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.basePreset;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.basePreset);
    m_settingsInfoMap.Insert(2924533825, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.interfaceLoggerConfig.elevatedPreset;
    info.valueSize = sizeof(m_settings.interfaceLoggerConfig.elevatedPreset);
    m_settingsInfoMap.Insert(4040226650, info);

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
            component.settingsDataHeader.isEncoded = true;
            component.settingsDataHeader.magicBufferId = 402778310;
            component.settingsDataHeader.magicBufferOffset = 0;

            pSettingsService->RegisterComponent(component);
        }
    }
}

} // Pal
