/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palAssert.h"
#include "palDbgPrint.h"
#include "core/platform.h"
#include "core/g_palPlatformSettings.h"

namespace Pal
{

class Device;
class Platform;

// =====================================================================================================================
// This class is responsible for loading the PAL Platform settings structure specified in the constructor
class PlatformSettingsLoader : public ISettingsLoader
{
public:
    explicit PlatformSettingsLoader(Pal::Platform* pPlatform);
    virtual ~PlatformSettingsLoader();

    virtual Result Init() override;

    const PalPlatformSettings& GetSettings() const { return m_settings; }
    PalPlatformSettings* GetSettingsPtr() { return &m_settings; }

    void OverrideDefaults();

    // auto-generated function
    void ReadSettings(Pal::Device* pDevice);

protected:
    virtual DevDriver::Result PerformSetValue(
        SettingNameHash     hash,
        const SettingValue& settingValue) override;

    void ValidateSettings();

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(PlatformSettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(PlatformSettingsLoader);

    // Generate the settings hash which is based on HW-specific setting.
    void GenerateSettingHash();

    #if PAL_ENABLE_PRINTS_ASSERTS
        void ReadAssertAndPrintSettings(Pal::Device* pDevice);
    #endif

    Pal::Platform*       m_pPlatform;
    PalPlatformSettings  m_settings;

    // This base class function will be empty for the platform settings. Instead a separate function is defined that
    // will take a device pointer as a parameter which is required for reading from the registry or settings file.
    virtual void ReadSettings() override {};

    // auto-generated functions
    virtual void SetupDefaults() override;
    virtual void InitSettingsInfo() override;
    virtual void DevDriverRegister() override;

    const char*const m_pComponentName;
};

} // Pal
