/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "g_coreSettings.h"
#include "palDevice.h"
#include "palInlineFuncs.h"
#include "palMetroHash.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// This class is responsible for loading the PAL Core runtime settings structure specified in the constructor
class SettingsLoader : public DevDriver::SettingsBase
{
public:
    explicit SettingsLoader(Device* pDevice);
    virtual ~SettingsLoader();

    Result Init();
    void FinalizeSettings();

    const PalSettings& GetSettings() const { return m_settings; }
    PalSettings* GetSettingsPtr() { return &m_settings; }

    Util::MetroHash::Hash GetSettingsHash() const { return m_settingsHash; };

    // auto-generated functions
    virtual uint64 GetSettingsBlobHash() const override;

protected:
    void ValidateSettings();

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);

    bool ReadSetting(
        const char*          pSettingName,
        Util::ValueType      valueType,
        void*                pValue,
        InternalSettingScope settingType,
        size_t               bufferSize = 0);

    // Generate the settings hash which is based on HW-specific setting.
    void GenerateSettingHash();

    void OverrideDefaults();

#if PAL_ENABLE_PRINTS_ASSERTS
    void InitDpLevelSettings();
#endif

    Device* m_pDevice;
    PalSettings m_settings;
    Util::MetroHash::Hash m_settingsHash;

    // auto-generated functions
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;
    virtual void ReadSettings() override;
};

} // Pal
