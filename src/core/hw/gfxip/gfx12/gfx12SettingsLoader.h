/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "dd_settings_base.h"
#include "g_gfx12Settings.h"

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// This class is responsible for loading the PAL Gfx12 runtime settings structure specified in the constructor
class SettingsLoader : public DevDriver::SettingsBase
{
public:
    explicit SettingsLoader(Pal::Device* pDevice);
    virtual ~SettingsLoader();

    Result Init();

    const Pal::Gfx12::Gfx12PalSettings& GetSettings() const { return m_settings; }

    void ValidateSettings(PalSettings* pSettings);
    void OverrideDefaults(PalSettings* pSettings);

    Util::MetroHash::Hash GetSettingsHash() const { return m_settingsHash; }

    // auto-generated functions
    virtual uint64 GetSettingsBlobHash() const override;
    virtual void ReadSettings() override;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);

    bool ReadSetting(
        const char*          pSettingName,
        Util::ValueType      valueType,
        void*                pValue,
        InternalSettingScope settingType,
        size_t               bufferSize = 0);

    // Generate the settings hash which is based on Gfx12-specific setting.
    void GenerateSettingHash();

    Pal::Device*                 m_pDevice;
    Pal::Gfx12::Gfx12PalSettings m_settings;
    Util::MetroHash::Hash        m_settingsHash;

    // auto-generated functions
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;
};

} // Gfx12
} // Pal
