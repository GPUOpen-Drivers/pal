/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
// When changes are needed, modify the tools generating this module in the tools\generate directory OR
// settings_platform.json
//
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "core/device.h"
#include "core/settingsLoader.h"
#include "core/g_palSettings.h"
#include "palInlineFuncs.h"
#include "palHashMapImpl.h"

#include "devDriverServer.h"
#include "protocols/ddSettingsService.h"

using namespace DevDriver::SettingsURIService;

namespace Pal
{

// =====================================================================================================================
// Initializes the settings structure to default values.
void SettingsLoader::SetupDefaults()
{
    // set setting variables to their default values...
    m_settings.textureOptLevel = 1;
    m_settings.catalystAI = 1;
    m_settings.forcePreambleCmdStream = true;
    m_settings.maxNumCmdStreamsPerSubmit = 0;
    m_settings.requestHighPriorityVmid = false;
    m_settings.requestDebugVmid = false;
    m_settings.neverChangeClockMode = false;
    m_settings.nonlocalDestGraphicsCopyRbs = 0;
    m_settings.ifh = IfhModeDisabled;
    m_settings.idleAfterSubmitGpuMask = 0x0;
    m_settings.tossPointMode = TossPointNone;
    m_settings.forceFixedFuncColorResolve = false;
    m_settings.unboundDescriptorAddress = 0xdeadbeefdeadbeef;
    m_settings.isLocalHeapPreferred = false;
    m_settings.enableNullCpuAccessFlag = false;
    m_settings.clearAllocatedLfb = false;
    m_settings.enableAdaptiveSync = false;
    m_settings.addr2Disable256BSwizzleMode = false;
    m_settings.addr2Disable4kBSwizzleMode = 0x0;
    m_settings.addr2UseVarSwizzleMode = 0x2;

    m_settings.rpmViewsBypassMall = 0x0;

    m_settings.enableBigPagePreAlignment = true;
    m_settings.enableIterate256PreAlignment = true;
    m_settings.addr2DisableSModes8BppColor = false;
#if   (__unix__)
    m_settings.disableOptimizedDisplay = true;
#else
    m_settings.disableOptimizedDisplay = false;
#endif
    m_settings.overlayReportHDR = true;
    m_settings.preferredPipelineUploadHeap = PipelineHeapDeferToClient;
#if PAL_DEVELOPER_BUILD
    m_settings.insertGuardPageBetweenWddm2VAs = false;
#endif

    m_settings.allocationListReusable = true;
    m_settings.cmdStreamReadOnly = false;
    m_settings.fenceTimeoutOverrideInSec = 0;
    m_settings.force64kPageGranularity = false;
    m_settings.updateOneGpuVirtualAddress = false;
    m_settings.alwaysResident = false;
    m_settings.disableSyncobjFence = false;
    m_settings.disableSdmaEngine = false;
    m_settings.forceT2tScanlineCopies = false;
    m_settings.enableVmAlwaysValid = VmAlwaysValidDefaultEnable;
    m_settings.disableSyncObject = false;
    m_settings.cmdBufDumpMode = CmdBufDumpModeDisabled;
    m_settings.cmdBufDumpFormat = CmdBufDumpFormatText;
#if   (__unix__)
    memset(m_settings.cmdBufDumpDirectory, 0, 512);
    strncpy(m_settings.cmdBufDumpDirectory, "amdpal/", 512);
#else
    memset(m_settings.cmdBufDumpDirectory, 0, 512);
    strncpy(m_settings.cmdBufDumpDirectory, "amdpal/", 512);
#endif
    m_settings.submitTimeCmdBufDumpStartFrame = 0;
    m_settings.submitTimeCmdBufDumpEndFrame = 0;
    m_settings.dumpCmdBufPerFrame = true;
    m_settings.logCmdBufCommitSizes = false;
    m_settings.logPipelineElf = false;
    m_settings.pipelineElfLogConfig.logInternal = false;
    m_settings.pipelineElfLogConfig.logExternal = false;
    m_settings.pipelineElfLogConfig.logHash = 0x0;
#if   (__unix__)
    memset(m_settings.pipelineElfLogConfig.logDirectory, 0, 512);
    strncpy(m_settings.pipelineElfLogConfig.logDirectory, "amdpal/", 512);
#else
    memset(m_settings.pipelineElfLogConfig.logDirectory, 0, 512);
    strncpy(m_settings.pipelineElfLogConfig.logDirectory, "amdpal/", 512);
#endif
    m_settings.cmdStreamReserveLimit = 256;
    m_settings.cmdStreamEnableMemsetOnReserve = false;
    m_settings.cmdStreamMemsetValue = 4294967295;
    m_settings.cmdBufChunkEnableStagingBuffer = false;
    m_settings.cmdBufDisallowNestedLaunchViaIb2 = false;
    m_settings.cmdAllocatorFreeOnReset = false;
    m_settings.cmdBufOptimizePm4 = Pm4OptDefaultEnable;
    m_settings.cmdBufForceOneTimeSubmit = CmdBufForceOneTimeSubmitDefault;
    m_settings.cmdBufPreemptionMode = CmdBufPreemptModeEnable;
    m_settings.commandBufferForceCeRamDumpInPostamble = false;
    m_settings.commandBufferCombineDePreambles = false;
    m_settings.videoCommandBufferAddVerifySignature = false;
    m_settings.cmdUtilVerifyShadowedRegRanges = true;
    m_settings.submitOptModeOverride = 0;
    m_settings.tileSwizzleMode = 0x7;
    m_settings.enableVidMmGpuVaMappingValidation = false;
    m_settings.enableUswcHeapAllAllocations = false;
    m_settings.addr2PreferredSwizzleTypeSet = Addr2PreferredDefault;
    m_settings.pipelinePrefetchEnable = true;
    m_settings.shaderPrefetchClampSize = 0;
    m_settings.maxAvailableSgpr = 0;
    m_settings.maxAvailableVgpr = 0;
    m_settings.maxThreadGroupsPerComputeUnit = 0;
    m_settings.maxScratchRingSizeBaseline = 268435456;
    m_settings.maxScratchRingScalePct = 10;
    m_settings.ifhGpuMask = 0xf;
    m_settings.hwCompositingEnabled = true;
    m_settings.mgpuCompatibilityEnabled = true;
    m_settings.peerMemoryEnabled = true;
    m_settings.forcePresentViaCpuBlt = false;
    m_settings.presentViaOglRuntime = true;

    m_settings.debugForceSurfaceAlignment = 0;
    m_settings.debugForceResourceAdditionalPadding = 0;
    m_settings.overlayReportMes = true;
    m_settings.mipGenUseFastPath = false;
    m_settings.useFp16GenMips = false;
    m_settings.tmzEnabled = true;
#if PAL_DEVELOPER_BUILD
    m_settings.dbgHelperBits = 0x0;
#endif

    m_settings.useDcc = 0x0;
    m_settings.numSettings = g_palNumSettings;
}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
void SettingsLoader::ReadSettings()
{
    // read from the OS adapter for each individual setting
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTFQStr,
                           Util::ValueType::Uint,
                           &m_settings.textureOptLevel,
                           InternalSettingScope::PublicCatalystKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCatalystAIStr,
                           Util::ValueType::Uint,
                           &m_settings.catalystAI,
                           InternalSettingScope::PublicCatalystKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForcePreambleCmdStreamStr,
                           Util::ValueType::Boolean,
                           &m_settings.forcePreambleCmdStream,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxNumCmdStreamsPerSubmitStr,
                           Util::ValueType::Uint,
                           &m_settings.maxNumCmdStreamsPerSubmit,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pRequestHighPriorityVmidStr,
                           Util::ValueType::Boolean,
                           &m_settings.requestHighPriorityVmid,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pRequestDebugVmidStr,
                           Util::ValueType::Boolean,
                           &m_settings.requestDebugVmid,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNeverChangeClockModeStr,
                           Util::ValueType::Boolean,
                           &m_settings.neverChangeClockMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pNonlocalDestGraphicsCopyRbsStr,
                           Util::ValueType::Int,
                           &m_settings.nonlocalDestGraphicsCopyRbs,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIFHStr,
                           Util::ValueType::Uint,
                           &m_settings.ifh,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIdleAfterSubmitGpuMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.idleAfterSubmitGpuMask,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTossPointModeStr,
                           Util::ValueType::Uint,
                           &m_settings.tossPointMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceFixedFuncColorResolveStr,
                           Util::ValueType::Boolean,
                           &m_settings.forceFixedFuncColorResolve,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUnboundDescriptorAddressStr,
                           Util::ValueType::Uint64,
                           &m_settings.unboundDescriptorAddress,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIsLocalHeapPreferredStr,
                           Util::ValueType::Boolean,
                           &m_settings.isLocalHeapPreferred,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableNullCpuAccessFlagStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableNullCpuAccessFlag,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pClearAllocatedLfbStr,
                           Util::ValueType::Boolean,
                           &m_settings.clearAllocatedLfb,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableAdaptiveSyncStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableAdaptiveSync,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2Disable256BSwizzleModeStr,
                           Util::ValueType::Boolean,
                           &m_settings.addr2Disable256BSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2Disable4KbSwizzleModeStr,
                           Util::ValueType::Uint,
                           &m_settings.addr2Disable4kBSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2UseVarSwizzleModeStr,
                           Util::ValueType::Uint,
                           &m_settings.addr2UseVarSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pRpmViewsBypassMallStr,
                           Util::ValueType::Uint,
                           &m_settings.rpmViewsBypassMall,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(penableBigPagePreAlignmentStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableBigPagePreAlignment,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(penableIterate256PreAlignmentStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableIterate256PreAlignment,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2DisableSModes8BppColorStr,
                           Util::ValueType::Boolean,
                           &m_settings.addr2DisableSModes8BppColor,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableOptimizedDisplayStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableOptimizedDisplay,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOverlayReportHDRStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayReportHDR,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPreferredPipelineUploadHeapStr,
                           Util::ValueType::Uint,
                           &m_settings.preferredPipelineUploadHeap,
                           InternalSettingScope::PrivatePalKey);

#if PAL_DEVELOPER_BUILD
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pInsertGuardPageBetweenWddm2VAsStr,
                           Util::ValueType::Boolean,
                           &m_settings.insertGuardPageBetweenWddm2VAs,
                           InternalSettingScope::PrivatePalKey);
#endif

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAllocationListReusableStr,
                           Util::ValueType::Boolean,
                           &m_settings.allocationListReusable,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdStreamReadOnlyStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdStreamReadOnly,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pFenceTimeoutOverrideStr,
                           Util::ValueType::Uint,
                           &m_settings.fenceTimeoutOverrideInSec,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForce64kPageGranularityStr,
                           Util::ValueType::Boolean,
                           &m_settings.force64kPageGranularity,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUpdateOneGpuVirtualAddressStr,
                           Util::ValueType::Boolean,
                           &m_settings.updateOneGpuVirtualAddress,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAlwaysResidentStr,
                           Util::ValueType::Boolean,
                           &m_settings.alwaysResident,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableSyncobjFenceStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableSyncobjFence,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableSdmaEngineStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableSdmaEngine,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForceT2tScanlineCopiesStr,
                           Util::ValueType::Boolean,
                           &m_settings.forceT2tScanlineCopies,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableVmAlwaysValidStr,
                           Util::ValueType::Uint,
                           &m_settings.enableVmAlwaysValid,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDisableSyncObjectStr,
                           Util::ValueType::Boolean,
                           &m_settings.disableSyncObject,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufDumpModeStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufDumpMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufDumpFormatStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufDumpFormat,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufDumpDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.cmdBufDumpDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSubmitTimeCmdBufDumpStartFrameStr,
                           Util::ValueType::Uint,
                           &m_settings.submitTimeCmdBufDumpStartFrame,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSubmitTimeCmdBufDumpEndFrameStr,
                           Util::ValueType::Uint,
                           &m_settings.submitTimeCmdBufDumpEndFrame,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDumpCmdBufPerFrameStr,
                           Util::ValueType::Boolean,
                           &m_settings.dumpCmdBufPerFrame,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLogCmdBufCommitSizesStr,
                           Util::ValueType::Boolean,
                           &m_settings.logCmdBufCommitSizes,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pLogPipelineElfStr,
                           Util::ValueType::Boolean,
                           &m_settings.logPipelineElf,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPipelineElfLogConfig_LogInternalStr,
                           Util::ValueType::Boolean,
                           &m_settings.pipelineElfLogConfig.logInternal,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPipelineElfLogConfig_LogExternalStr,
                           Util::ValueType::Boolean,
                           &m_settings.pipelineElfLogConfig.logExternal,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPipelineElfLogConfig_LogHashStr,
                           Util::ValueType::Uint64,
                           &m_settings.pipelineElfLogConfig.logHash,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPipelineElfLogConfig_LogDirectoryStr,
                           Util::ValueType::Str,
                           &m_settings.pipelineElfLogConfig.logDirectory,
                           InternalSettingScope::PrivatePalKey,
                           512);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdStreamReserveLimitStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdStreamReserveLimit,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdStreamEnableMemsetOnReserveStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdStreamEnableMemsetOnReserve,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdStreamMemsetValueStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdStreamMemsetValue,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufChunkEnableStagingBufferStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdBufChunkEnableStagingBuffer,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufDisallowNestedLaunchViaIb2Str,
                           Util::ValueType::Boolean,
                           &m_settings.cmdBufDisallowNestedLaunchViaIb2,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdAllocatorFreeOnResetStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdAllocatorFreeOnReset,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufOptimizePm4Str,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufOptimizePm4,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufForceOneTimeSubmitStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufForceOneTimeSubmit,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdBufPreemptionModeStr,
                           Util::ValueType::Uint,
                           &m_settings.cmdBufPreemptionMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCommandBufferForceCeRamDumpInPostambleStr,
                           Util::ValueType::Boolean,
                           &m_settings.commandBufferForceCeRamDumpInPostamble,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCommandBufferCombineDePreamblesStr,
                           Util::ValueType::Boolean,
                           &m_settings.commandBufferCombineDePreambles,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pVideoCommandBufferAddVerifySignatureStr,
                           Util::ValueType::Boolean,
                           &m_settings.videoCommandBufferAddVerifySignature,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pCmdUtilVerifyShadowedRegRangesStr,
                           Util::ValueType::Boolean,
                           &m_settings.cmdUtilVerifyShadowedRegRanges,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pSubmitOptModeOverrideStr,
                           Util::ValueType::Uint,
                           &m_settings.submitOptModeOverride,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTileSwizzleModeStr,
                           Util::ValueType::Uint,
                           &m_settings.tileSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableVidMmGpuVaMappingValidationStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableVidMmGpuVaMappingValidation,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pEnableUswcHeapAllAllocationsStr,
                           Util::ValueType::Boolean,
                           &m_settings.enableUswcHeapAllAllocations,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2PreferredSwizzleTypeSetStr,
                           Util::ValueType::Uint,
                           &m_settings.addr2PreferredSwizzleTypeSet,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPipelinePrefetchEnableStr,
                           Util::ValueType::Boolean,
                           &m_settings.pipelinePrefetchEnable,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pShaderPrefetchClampSizeStr,
                           Util::ValueType::Uint,
                           &m_settings.shaderPrefetchClampSize,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxAvailableSgprStr,
                           Util::ValueType::Uint,
                           &m_settings.maxAvailableSgpr,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxAvailableVgprStr,
                           Util::ValueType::Uint,
                           &m_settings.maxAvailableVgpr,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxThreadGroupsPerComputeUnitStr,
                           Util::ValueType::Uint,
                           &m_settings.maxThreadGroupsPerComputeUnit,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxScratchRingSizeBaselineStr,
                           Util::ValueType::Uint64,
                           &m_settings.maxScratchRingSizeBaseline,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMaxScratchRingSizeScalePctStr,
                           Util::ValueType::Uint,
                           &m_settings.maxScratchRingScalePct,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pIfhGpuMaskStr,
                           Util::ValueType::Uint,
                           &m_settings.ifhGpuMask,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pHwCompositingEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.hwCompositingEnabled,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMgpuCompatibilityEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.mgpuCompatibilityEnabled,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPeerMemoryEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.peerMemoryEnabled,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pForcePresentViaCpuBltStr,
                           Util::ValueType::Boolean,
                           &m_settings.forcePresentViaCpuBlt,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pPresentViaOglRuntimeStr,
                           Util::ValueType::Boolean,
                           &m_settings.presentViaOglRuntime,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDebugForceResourceAlignmentStr,
                           Util::ValueType::Uint64,
                           &m_settings.debugForceSurfaceAlignment,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDebugForceResourceAdditionalPaddingStr,
                           Util::ValueType::Uint64,
                           &m_settings.debugForceResourceAdditionalPadding,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pOverlayReportMesStr,
                           Util::ValueType::Boolean,
                           &m_settings.overlayReportMes,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMipGenUseFastPathStr,
                           Util::ValueType::Boolean,
                           &m_settings.mipGenUseFastPath,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseFp16GenMipsStr,
                           Util::ValueType::Boolean,
                           &m_settings.useFp16GenMips,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pTmzEnabledStr,
                           Util::ValueType::Boolean,
                           &m_settings.tmzEnabled,
                           InternalSettingScope::PrivatePalKey);

#if PAL_DEVELOPER_BUILD
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pDbgHelperBitsStr,
                           Util::ValueType::Uint64,
                           &m_settings.dbgHelperBits,
                           InternalSettingScope::PrivatePalKey);
#endif

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseDccStr,
                           Util::ValueType::Uint,
                           &m_settings.useDcc,
                           InternalSettingScope::PrivatePalKey);

}

// =====================================================================================================================
// Reads the setting from the OS adapter and sets the structure value when the setting values are found.
// This is expected to be done after the component has perform overrides of any defaults.
void SettingsLoader::RereadSettings()
{
    // read from the OS adapter for each individual setting
    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2Disable256BSwizzleModeStr,
                           Util::ValueType::Boolean,
                           &m_settings.addr2Disable256BSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pAddr2UseVarSwizzleModeStr,
                           Util::ValueType::Uint,
                           &m_settings.addr2UseVarSwizzleMode,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pMipGenUseFastPathStr,
                           Util::ValueType::Boolean,
                           &m_settings.mipGenUseFastPath,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseFp16GenMipsStr,
                           Util::ValueType::Boolean,
                           &m_settings.useFp16GenMips,
                           InternalSettingScope::PrivatePalKey);

    static_cast<Pal::Device*>(m_pDevice)->ReadSetting(pUseDccStr,
                           Util::ValueType::Uint,
                           &m_settings.useDcc,
                           InternalSettingScope::PrivatePalKey);

}

// =====================================================================================================================
// Initializes the SettingInfo hash map and array of setting hashes.
void SettingsLoader::InitSettingsInfo()
{
    SettingInfo info = {};

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.textureOptLevel;
    info.valueSize = sizeof(m_settings.textureOptLevel);
    m_settingsInfoMap.Insert(4265240458, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.catalystAI;
    info.valueSize = sizeof(m_settings.catalystAI);
    m_settingsInfoMap.Insert(1901986348, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forcePreambleCmdStream;
    info.valueSize = sizeof(m_settings.forcePreambleCmdStream);
    m_settingsInfoMap.Insert(2987947496, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.maxNumCmdStreamsPerSubmit;
    info.valueSize = sizeof(m_settings.maxNumCmdStreamsPerSubmit);
    m_settingsInfoMap.Insert(2467045849, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.requestHighPriorityVmid;
    info.valueSize = sizeof(m_settings.requestHighPriorityVmid);
    m_settingsInfoMap.Insert(1580739202, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.requestDebugVmid;
    info.valueSize = sizeof(m_settings.requestDebugVmid);
    m_settingsInfoMap.Insert(359792145, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.neverChangeClockMode;
    info.valueSize = sizeof(m_settings.neverChangeClockMode);
    m_settingsInfoMap.Insert(2936106678, info);

    info.type      = SettingType::Int;
    info.pValuePtr = &m_settings.nonlocalDestGraphicsCopyRbs;
    info.valueSize = sizeof(m_settings.nonlocalDestGraphicsCopyRbs);
    m_settingsInfoMap.Insert(501901000, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.ifh;
    info.valueSize = sizeof(m_settings.ifh);
    m_settingsInfoMap.Insert(3299864138, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.idleAfterSubmitGpuMask;
    info.valueSize = sizeof(m_settings.idleAfterSubmitGpuMask);
    m_settingsInfoMap.Insert(2665794079, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.tossPointMode;
    info.valueSize = sizeof(m_settings.tossPointMode);
    m_settingsInfoMap.Insert(440136999, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forceFixedFuncColorResolve;
    info.valueSize = sizeof(m_settings.forceFixedFuncColorResolve);
    m_settingsInfoMap.Insert(4239167273, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.unboundDescriptorAddress;
    info.valueSize = sizeof(m_settings.unboundDescriptorAddress);
    m_settingsInfoMap.Insert(2972919517, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.isLocalHeapPreferred;
    info.valueSize = sizeof(m_settings.isLocalHeapPreferred);
    m_settingsInfoMap.Insert(1465087975, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableNullCpuAccessFlag;
    info.valueSize = sizeof(m_settings.enableNullCpuAccessFlag);
    m_settingsInfoMap.Insert(3709502715, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.clearAllocatedLfb;
    info.valueSize = sizeof(m_settings.clearAllocatedLfb);
    m_settingsInfoMap.Insert(2657420565, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableAdaptiveSync;
    info.valueSize = sizeof(m_settings.enableAdaptiveSync);
    m_settingsInfoMap.Insert(1325234467, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.addr2Disable256BSwizzleMode;
    info.valueSize = sizeof(m_settings.addr2Disable256BSwizzleMode);
    m_settingsInfoMap.Insert(2981505442, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.addr2Disable4kBSwizzleMode;
    info.valueSize = sizeof(m_settings.addr2Disable4kBSwizzleMode);
    m_settingsInfoMap.Insert(2252676842, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.addr2UseVarSwizzleMode;
    info.valueSize = sizeof(m_settings.addr2UseVarSwizzleMode);
    m_settingsInfoMap.Insert(2076875821, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.rpmViewsBypassMall;
    info.valueSize = sizeof(m_settings.rpmViewsBypassMall);
    m_settingsInfoMap.Insert(2274774246, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableBigPagePreAlignment;
    info.valueSize = sizeof(m_settings.enableBigPagePreAlignment);
    m_settingsInfoMap.Insert(3301250889, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableIterate256PreAlignment;
    info.valueSize = sizeof(m_settings.enableIterate256PreAlignment);
    m_settingsInfoMap.Insert(2507710515, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.addr2DisableSModes8BppColor;
    info.valueSize = sizeof(m_settings.addr2DisableSModes8BppColor);
    m_settingsInfoMap.Insert(3379142860, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableOptimizedDisplay;
    info.valueSize = sizeof(m_settings.disableOptimizedDisplay);
    m_settingsInfoMap.Insert(3371140286, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayReportHDR;
    info.valueSize = sizeof(m_settings.overlayReportHDR);
    m_settingsInfoMap.Insert(2354711641, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.preferredPipelineUploadHeap;
    info.valueSize = sizeof(m_settings.preferredPipelineUploadHeap);
    m_settingsInfoMap.Insert(1170638299, info);
#if PAL_DEVELOPER_BUILD

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.insertGuardPageBetweenWddm2VAs;
    info.valueSize = sizeof(m_settings.insertGuardPageBetweenWddm2VAs);
    m_settingsInfoMap.Insert(3303637006, info);
#endif

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.allocationListReusable;
    info.valueSize = sizeof(m_settings.allocationListReusable);
    m_settingsInfoMap.Insert(1727036994, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdStreamReadOnly;
    info.valueSize = sizeof(m_settings.cmdStreamReadOnly);
    m_settingsInfoMap.Insert(3519117785, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.fenceTimeoutOverrideInSec;
    info.valueSize = sizeof(m_settings.fenceTimeoutOverrideInSec);
    m_settingsInfoMap.Insert(970172817, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.force64kPageGranularity;
    info.valueSize = sizeof(m_settings.force64kPageGranularity);
    m_settingsInfoMap.Insert(1833432496, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.updateOneGpuVirtualAddress;
    info.valueSize = sizeof(m_settings.updateOneGpuVirtualAddress);
    m_settingsInfoMap.Insert(4178383571, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.alwaysResident;
    info.valueSize = sizeof(m_settings.alwaysResident);
    m_settingsInfoMap.Insert(198913068, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableSyncobjFence;
    info.valueSize = sizeof(m_settings.disableSyncobjFence);
    m_settingsInfoMap.Insert(1287715858, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableSdmaEngine;
    info.valueSize = sizeof(m_settings.disableSdmaEngine);
    m_settingsInfoMap.Insert(2254617940, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forceT2tScanlineCopies;
    info.valueSize = sizeof(m_settings.forceT2tScanlineCopies);
    m_settingsInfoMap.Insert(2746133908, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.enableVmAlwaysValid;
    info.valueSize = sizeof(m_settings.enableVmAlwaysValid);
    m_settingsInfoMap.Insert(1718264096, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.disableSyncObject;
    info.valueSize = sizeof(m_settings.disableSyncObject);
    m_settingsInfoMap.Insert(830933859, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufDumpMode;
    info.valueSize = sizeof(m_settings.cmdBufDumpMode);
    m_settingsInfoMap.Insert(3607991033, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufDumpFormat;
    info.valueSize = sizeof(m_settings.cmdBufDumpFormat);
    m_settingsInfoMap.Insert(1905164977, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.cmdBufDumpDirectory;
    info.valueSize = sizeof(m_settings.cmdBufDumpDirectory);
    m_settingsInfoMap.Insert(3293295025, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.submitTimeCmdBufDumpStartFrame;
    info.valueSize = sizeof(m_settings.submitTimeCmdBufDumpStartFrame);
    m_settingsInfoMap.Insert(1639305458, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.submitTimeCmdBufDumpEndFrame;
    info.valueSize = sizeof(m_settings.submitTimeCmdBufDumpEndFrame);
    m_settingsInfoMap.Insert(4221961293, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.dumpCmdBufPerFrame;
    info.valueSize = sizeof(m_settings.dumpCmdBufPerFrame);
    m_settingsInfoMap.Insert(653867010, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.logCmdBufCommitSizes;
    info.valueSize = sizeof(m_settings.logCmdBufCommitSizes);
    m_settingsInfoMap.Insert(2222002517, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.logPipelineElf;
    info.valueSize = sizeof(m_settings.logPipelineElf);
    m_settingsInfoMap.Insert(2287487712, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.pipelineElfLogConfig.logInternal;
    info.valueSize = sizeof(m_settings.pipelineElfLogConfig.logInternal);
    m_settingsInfoMap.Insert(2576934177, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.pipelineElfLogConfig.logExternal;
    info.valueSize = sizeof(m_settings.pipelineElfLogConfig.logExternal);
    m_settingsInfoMap.Insert(3434531143, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.pipelineElfLogConfig.logHash;
    info.valueSize = sizeof(m_settings.pipelineElfLogConfig.logHash);
    m_settingsInfoMap.Insert(1952137328, info);

    info.type      = SettingType::String;
    info.pValuePtr = &m_settings.pipelineElfLogConfig.logDirectory;
    info.valueSize = sizeof(m_settings.pipelineElfLogConfig.logDirectory);
    m_settingsInfoMap.Insert(1171198787, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdStreamReserveLimit;
    info.valueSize = sizeof(m_settings.cmdStreamReserveLimit);
    m_settingsInfoMap.Insert(3843913604, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdStreamEnableMemsetOnReserve;
    info.valueSize = sizeof(m_settings.cmdStreamEnableMemsetOnReserve);
    m_settingsInfoMap.Insert(3927521274, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdStreamMemsetValue;
    info.valueSize = sizeof(m_settings.cmdStreamMemsetValue);
    m_settingsInfoMap.Insert(3661455441, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdBufChunkEnableStagingBuffer;
    info.valueSize = sizeof(m_settings.cmdBufChunkEnableStagingBuffer);
    m_settingsInfoMap.Insert(169161685, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdBufDisallowNestedLaunchViaIb2;
    info.valueSize = sizeof(m_settings.cmdBufDisallowNestedLaunchViaIb2);
    m_settingsInfoMap.Insert(459136606, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdAllocatorFreeOnReset;
    info.valueSize = sizeof(m_settings.cmdAllocatorFreeOnReset);
    m_settingsInfoMap.Insert(1461164706, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufOptimizePm4;
    info.valueSize = sizeof(m_settings.cmdBufOptimizePm4);
    m_settingsInfoMap.Insert(1018895288, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufForceOneTimeSubmit;
    info.valueSize = sizeof(m_settings.cmdBufForceOneTimeSubmit);
    m_settingsInfoMap.Insert(909934676, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.cmdBufPreemptionMode;
    info.valueSize = sizeof(m_settings.cmdBufPreemptionMode);
    m_settingsInfoMap.Insert(3640527208, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.commandBufferForceCeRamDumpInPostamble;
    info.valueSize = sizeof(m_settings.commandBufferForceCeRamDumpInPostamble);
    m_settingsInfoMap.Insert(3413911781, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.commandBufferCombineDePreambles;
    info.valueSize = sizeof(m_settings.commandBufferCombineDePreambles);
    m_settingsInfoMap.Insert(148412311, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.videoCommandBufferAddVerifySignature;
    info.valueSize = sizeof(m_settings.videoCommandBufferAddVerifySignature);
    m_settingsInfoMap.Insert(3347736595, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.cmdUtilVerifyShadowedRegRanges;
    info.valueSize = sizeof(m_settings.cmdUtilVerifyShadowedRegRanges);
    m_settingsInfoMap.Insert(3890704045, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.submitOptModeOverride;
    info.valueSize = sizeof(m_settings.submitOptModeOverride);
    m_settingsInfoMap.Insert(3054810609, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.tileSwizzleMode;
    info.valueSize = sizeof(m_settings.tileSwizzleMode);
    m_settingsInfoMap.Insert(1146877010, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableVidMmGpuVaMappingValidation;
    info.valueSize = sizeof(m_settings.enableVidMmGpuVaMappingValidation);
    m_settingsInfoMap.Insert(2751785051, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.enableUswcHeapAllAllocations;
    info.valueSize = sizeof(m_settings.enableUswcHeapAllAllocations);
    m_settingsInfoMap.Insert(3408333164, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.addr2PreferredSwizzleTypeSet;
    info.valueSize = sizeof(m_settings.addr2PreferredSwizzleTypeSet);
    m_settingsInfoMap.Insert(1836557167, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.pipelinePrefetchEnable;
    info.valueSize = sizeof(m_settings.pipelinePrefetchEnable);
    m_settingsInfoMap.Insert(3800985923, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.shaderPrefetchClampSize;
    info.valueSize = sizeof(m_settings.shaderPrefetchClampSize);
    m_settingsInfoMap.Insert(2406290039, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.maxAvailableSgpr;
    info.valueSize = sizeof(m_settings.maxAvailableSgpr);
    m_settingsInfoMap.Insert(1008439776, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.maxAvailableVgpr;
    info.valueSize = sizeof(m_settings.maxAvailableVgpr);
    m_settingsInfoMap.Insert(2116546305, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.maxThreadGroupsPerComputeUnit;
    info.valueSize = sizeof(m_settings.maxThreadGroupsPerComputeUnit);
    m_settingsInfoMap.Insert(1284517999, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.maxScratchRingSizeBaseline;
    info.valueSize = sizeof(m_settings.maxScratchRingSizeBaseline);
    m_settingsInfoMap.Insert(913921073, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.maxScratchRingScalePct;
    info.valueSize = sizeof(m_settings.maxScratchRingScalePct);
    m_settingsInfoMap.Insert(3497759531, info);

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.ifhGpuMask;
    info.valueSize = sizeof(m_settings.ifhGpuMask);
    m_settingsInfoMap.Insert(3517626664, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.hwCompositingEnabled;
    info.valueSize = sizeof(m_settings.hwCompositingEnabled);
    m_settingsInfoMap.Insert(1872169717, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.mgpuCompatibilityEnabled;
    info.valueSize = sizeof(m_settings.mgpuCompatibilityEnabled);
    m_settingsInfoMap.Insert(1177937299, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.peerMemoryEnabled;
    info.valueSize = sizeof(m_settings.peerMemoryEnabled);
    m_settingsInfoMap.Insert(259362511, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.forcePresentViaCpuBlt;
    info.valueSize = sizeof(m_settings.forcePresentViaCpuBlt);
    m_settingsInfoMap.Insert(2055732513, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.presentViaOglRuntime;
    info.valueSize = sizeof(m_settings.presentViaOglRuntime);
    m_settingsInfoMap.Insert(2466363770, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.debugForceSurfaceAlignment;
    info.valueSize = sizeof(m_settings.debugForceSurfaceAlignment);
    m_settingsInfoMap.Insert(397089904, info);

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.debugForceResourceAdditionalPadding;
    info.valueSize = sizeof(m_settings.debugForceResourceAdditionalPadding);
    m_settingsInfoMap.Insert(3601080919, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.overlayReportMes;
    info.valueSize = sizeof(m_settings.overlayReportMes);
    m_settingsInfoMap.Insert(1685803860, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.mipGenUseFastPath;
    info.valueSize = sizeof(m_settings.mipGenUseFastPath);
    m_settingsInfoMap.Insert(3353227045, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.useFp16GenMips;
    info.valueSize = sizeof(m_settings.useFp16GenMips);
    m_settingsInfoMap.Insert(192229910, info);

    info.type      = SettingType::Boolean;
    info.pValuePtr = &m_settings.tmzEnabled;
    info.valueSize = sizeof(m_settings.tmzEnabled);
    m_settingsInfoMap.Insert(2606194033, info);
#if PAL_DEVELOPER_BUILD

    info.type      = SettingType::Uint64;
    info.pValuePtr = &m_settings.dbgHelperBits;
    info.valueSize = sizeof(m_settings.dbgHelperBits);
    m_settingsInfoMap.Insert(3894710420, info);
#endif

    info.type      = SettingType::Uint;
    info.pValuePtr = &m_settings.useDcc;
    info.valueSize = sizeof(m_settings.useDcc);
    m_settingsInfoMap.Insert(4029518654, info);

}

// =====================================================================================================================
// Registers the core settings with the Developer Driver settings service.
void SettingsLoader::DevDriverRegister()
{
    auto* pDevDriverServer = static_cast<Pal::Device*>(m_pDevice)->GetPlatform()->GetDevDriverServer();
    if (pDevDriverServer != nullptr)
    {
        auto* pSettingsService = pDevDriverServer->GetSettingsService();
        if (pSettingsService != nullptr)
        {
            RegisteredComponent component = {};
            strncpy(&component.componentName[0], m_pComponentName, kMaxComponentNameStrLen);
            component.pPrivateData = static_cast<void*>(this);
            component.pSettingsHashes = &g_palSettingHashList[0];
            component.numSettings = g_palNumSettings;
            component.pfnGetValue = ISettingsLoader::GetValue;
            component.pfnSetValue = ISettingsLoader::SetValue;
            component.pSettingsData = &g_palJsonData[0];
            component.settingsDataSize = sizeof(g_palJsonData);
            component.settingsDataHash = 480379498;
            component.settingsDataHeader.isEncoded = true;
            component.settingsDataHeader.magicBufferId = 402778310;
            component.settingsDataHeader.magicBufferOffset = 0;

            pSettingsService->RegisterComponent(component);
        }
    }
}

} // Pal
