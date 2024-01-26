/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "g_gfx9Settings.h"

namespace Pal
{

enum class GfxIpLevel : uint32;

namespace Gfx9
{

// =====================================================================================================================
// This class is responsible for loading the Gfx9-specific portion of the PalSettings
// structure specified in the constructor.  This is a helper class that only exists for a short
// time while the settings are initialized.
class SettingsLoader final : public DevDriver::SettingsBase
{
public:
    explicit SettingsLoader(Pal::Device* pDevice);
    virtual ~SettingsLoader();

    Result Init();

    const Gfx9PalSettings& GetSettings() const { return m_settings; }

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

    void GenerateSettingHash();

    // Private members
    Pal::Device*          m_pDevice;
    Gfx9PalSettings       m_settings;  ///< Gfx9 settings pointer
    const GfxIpLevel      m_gfxLevel;
    Util::MetroHash::Hash m_settingsHash;

    // auto-generated functions
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;
};

} // Gfx9
} // Pal
