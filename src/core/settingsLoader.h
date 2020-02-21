/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palSettingsLoader.h"
#include "core/g_palSettings.h"

namespace Pal
{

class Device;

// =====================================================================================================================
// This class is responsible for loading the PAL Core runtime settings structure specified in the constructor
class SettingsLoader : public ISettingsLoader
{
public:
    explicit SettingsLoader(Device* pDevice);
    virtual ~SettingsLoader();

    virtual Result Init() override;
    void FinalizeSettings();

    const PalSettings& GetSettings() const { return m_settings; };
    PalSettings* GetSettingsPtr() { return &m_settings; }

protected:
    void ValidateSettings();

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);

    void SetupHeapPerfRatings(PalSettings* pSettings);

    // Generate the settings hash which is based on HW-specific setting.
    void GenerateSettingHash();

    void OverrideDefaults();

    #if PAL_ENABLE_PRINTS_ASSERTS
        void InitDpLevelSettings();
    #endif

    Device*      m_pDevice;
    PalSettings  m_settings;

    // auto-generated functions
    virtual void SetupDefaults() override;
    virtual void ReadSettings() override;
    virtual void RereadSettings() override;
    virtual void InitSettingsInfo() override;
    virtual void DevDriverRegister() override;

    const char*const m_pComponentName;
};

} // Pal
