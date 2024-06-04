/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/devDriverUtil.h"
#include "core/device.h"
#include "core/settingsLoader.h"
#include "core/platform.h"

using namespace Util;

namespace Pal
{

// =====================================================================================================================
// Constructor for the SettingsLoader object.
SettingsLoader::SettingsLoader(
    Device* pDevice)
    :
    DevDriver::SettingsBase(&m_settings, sizeof(m_settings)),
    m_pDevice(pDevice)
{
}

// =====================================================================================================================
SettingsLoader::~SettingsLoader()
{
}

// =====================================================================================================================
// Initializes the environment settings to their default values
Result SettingsLoader::Init()
{
    DD_RESULT ddResult = SetupDefaultsAndPopulateMap();

    if (ddResult == DD_RESULT_SUCCESS)
    {
        // We want to override the default values for any platform specific reasons
        OverrideDefaults();

        // Read settings from Windows registry.
        ReadSettings();
    }

    return DdResultToPalResult(ddResult);
}

// =====================================================================================================================
bool SettingsLoader::ReadSetting(
    const char*          pSettingName,
    ValueType            valueType,
    void*                pValue,
    InternalSettingScope settingType,
    size_t               bufferSize)
{
    return m_pDevice->ReadSetting(
        pSettingName,
        valueType,
        pValue,
        InternalSettingScope::PrivatePalKey,
        bufferSize);
}

// =====================================================================================================================
// Overrides defaults for the settings based on runtime information.
void SettingsLoader::OverrideDefaults()
{
    m_pDevice->OverrideDefaultSettings(&m_settings);

    const GfxIpLevel gfxLevel = m_pDevice->ChipProperties().gfxLevel;

    if (IsGfx11(*m_pDevice))
    {
        m_settings.useDcc |= UseDccForAllCompatibleFormats;
    }

    // This is set by PAL based on when certain aspects of this feature were added to the uCode by PFP FW version
    // of this device. This setting is Read again from the Panel which means the stable value determined here can be
    // overridden by the value set in Panel/Registry settings.
    m_settings.useExecuteIndirectPacket = m_pDevice->ChipProperties().gfx9.executeIndirectSupport;

    if (IsNavi3x(*m_pDevice) && (m_settings.useExecuteIndirectPacket == UseExecuteIndirectV2Packet))
    {
        m_settings.useExecuteIndirectPacket = UseExecuteIndirectV1PacketForDrawDispatch;
    }

    if (m_pDevice->PhysicalEnginesAvailable())
    {
        // prevent exhausting invisible video memory due to excessive physical alignment for small allocations
        m_settings.enableUswcHeapAllAllocations = true;
    }

    // Since APUs don't have real local memory it's better to use a GART heap instead
    // of allocating out of the limited carveout space.
    if (m_pDevice->ChipProperties().gpuType == GpuType::Integrated)
    {
        m_settings.preferredPipelineUploadHeap = PipelineHeapGartUswc;
    }

    if (IsNavi2x(*m_pDevice)
       )
    {
        m_settings.addr2UseVarSwizzleMode = Addr2UseVarSwizzle::Addr2UseVarSwizzleDisable;
    }

    if (IsGfx103CorePlus(*m_pDevice))
    {
        m_settings.enableGangSubmit = true;
    }

    if ((m_pDevice->ChipProperties().vcnLevel != VcnIpLevel::_None)
        )
    {

        m_settings.waForceLinearHeight16Alignment = true;
    }

    if (m_pDevice->IsSpoofed())
    {
        // Sending commands intended for a spoofed GPU model to the different physical device is
        // almost certain to hang the device.
        if (m_settings.ifh == IfhModeDisabled)
        {
            m_settings.ifh = IfhModeKmd;
        }
    }
}

// =====================================================================================================================
// Validates that the settings structure has legal values and calls the Hwl method for validation. Variables that
// require complicated initialization can also be initialized here.
void SettingsLoader::ValidateSettings()
{
    m_pDevice->GetGfxDevice()->HwlValidateSettings(&m_settings);

    // If developer driver profiling is enabled, we should always request the debug-vmid/static-vmid and disable mid
    // command buffer preemption support.
    //
    // CrashAnalysis feature requires disablement of command buffer preemption, as well as DebugVmid.
    if (m_pDevice->GetPlatform()->IsDevDriverProfilingEnabled() ||
        m_pDevice->GetPlatform()->IsCrashAnalysisModeEnabled())
    {
        m_settings.requestDebugVmid     = true;
        m_settings.cmdBufPreemptionMode = CmdBufPreemptModeDisable;
    }

    // When tracing is enabled, we need to request debug/static VMID. This can be enabled via the DriverUtilsService.
    if (m_pDevice->GetPlatform()->IsTracingEnabled() ||
        m_pDevice->GetPlatform()->IsStaticVmidRequested())
    {
        m_settings.requestDebugVmid = true;
    }

    // Emulated devices may not be visible to the host OS, so use CPU paths to send data to the window system.
    if (m_pDevice->GetPlatform()->IsEmulationEnabled())
    {
        m_settings.forcePresentViaCpuBlt = true;
    }

    // Overrides all paths for debug files to expected values.
    // Now those directories in setting are all *relative*:
    // Relative to the path in the AMD_DEBUG_DIR environment variable, and if that env var isn't set, the location is
    // platform dependent. So we need to query the root path from device and then concatenate two strings (of the root
    // path and relative path of specific file) to the final usable absolute path
    const char* pRootPath = m_pDevice->GetDebugFilePath();
    auto* pPlatformSettings = m_pDevice->GetPlatform()->PlatformSettingsPtr();
    if (pRootPath != nullptr)
    {
        char subDir[DD_SETTINGS_MAX_PATH_SIZE];

        Strncpy(subDir, m_settings.cmdBufDumpDirectory, sizeof(subDir));

        Snprintf(m_settings.cmdBufDumpDirectory, sizeof(m_settings.cmdBufDumpDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, m_settings.pipelineElfLogConfig.logDirectory, sizeof(subDir));
        Snprintf(m_settings.pipelineElfLogConfig.logDirectory,
                 sizeof(m_settings.pipelineElfLogConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory,
                 sizeof(pPlatformSettings->overlayBenchmarkConfig.usageLogDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory,
                 sizeof(pPlatformSettings->overlayBenchmarkConfig.frameStatsLogDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->gpuProfilerConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->gpuProfilerConfig.logDirectory,
                 sizeof(pPlatformSettings->gpuProfilerConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->interfaceLoggerConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->interfaceLoggerConfig.logDirectory,
                 sizeof(pPlatformSettings->interfaceLoggerConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

        Strncpy(subDir, pPlatformSettings->pm4InstrumentorConfig.logDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->pm4InstrumentorConfig.logDirectory,
                 sizeof(pPlatformSettings->pm4InstrumentorConfig.logDirectory),
                 "%s/%s", pRootPath, subDir);

#if PAL_DEVELOPER_BUILD
        Strncpy(subDir, pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory, sizeof(subDir));
        Snprintf(pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory,
                 sizeof(pPlatformSettings->gpuDebugConfig.surfaceCaptureLogDirectory),
                 "%s/%s", pRootPath, subDir);
#endif

    }
}

// =====================================================================================================================
// Completes the initialization of the settings by overriding values from the registry and validating the final settings
// struct
void SettingsLoader::FinalizeSettings()
{
    ValidateSettings();
    GenerateSettingHash();
}

// =====================================================================================================================
// The settings hashes are used during pipeline loading to verify that the pipeline data is compatible between when it
// was stored and when it was loaded.  The CCC controls some of the settings though, and the CCC doesn't set it
// identically across all GPUs in an MGPU configuration.  Since the CCC keys don't affect pipeline generation, just
// ignore those values when it comes to hash generation.
void SettingsLoader::GenerateSettingHash()
{
    // Temporarily ignore these CCC settings when computing a settings hash as described in the function header.
    uint32 textureOptLevel = m_settings.tfq;
    m_settings.tfq = 0;

    MetroHash128::Hash(
        reinterpret_cast<const uint8*>(&m_settings),
        sizeof(PalSettings),
        m_settingsHash.bytes);

    m_settings.tfq = textureOptLevel;
}
} // Pal
