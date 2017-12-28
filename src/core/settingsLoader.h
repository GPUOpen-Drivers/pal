/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palSettings.h"
#include "palMetroHash.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// This class is responsible for loading the RuntimeSettings structure specified in the constructor.  This is a helper
// class that only exists for a short time while the settings are initialized.
class SettingsLoader
{
public:
    SettingsLoader(Device* pDevice, PalSettings* pSettings);
    virtual ~SettingsLoader() {}

    Result Init();

    void FinalizeSettings();

    const PalSettings* GetSettings() const { return m_pSettings; };

    Util::MetroHash::Hash GetSettingsHash() const { return m_settingHash; };

protected:
    void ValidateSettings();

    /// Optional HWL method initializes the HWL related environment settings
    virtual void HwlInit() { }

    /// Optional HWL method to read hardware specific registry
    virtual void HwlReadSettings() { }

    /// Optional HWL method to validate the settings
    virtual void HwlValidateSettings() { }

    /// Optional HWL method to override any default registry entries
    virtual void HwlOverrideDefaults() { }

    PalSettings*          m_pSettings;
    Device*               m_pDevice;
    Util::MetroHash::Hash m_settingHash;

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);

    void SetupHeapPerfRatings(PalSettings* pSettings);

    // Generate the settings hash which is based on HW-specific setting.
    virtual void GenerateSettingHash() = 0;

    void OverrideDefaults();

    #if PAL_ENABLE_PRINTS_ASSERTS
        void InitDpLevelSettings();
    #endif

    void InitEarlySettings();

    // auto-generated functions
    static void SetupDefaults(PalSettings* pSettings);
    void ReadSettings(PalSettings* pSettings);
};

} // Pal
