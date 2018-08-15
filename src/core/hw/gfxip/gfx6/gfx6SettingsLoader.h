/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/device.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"

namespace Util { class IndirectAllocator; }

namespace Pal
{

namespace Gfx6
{

// =====================================================================================================================
// This class is responsible for loading the Gfx6-specific portion of the PalSettings
// structure specified in the constructor.  This is a helper class that only exists for a short
// time while the settings are initialized.
class SettingsLoader : public Pal::ISettingsLoader
{
public:
    SettingsLoader(Util::IndirectAllocator* pAllocator, Pal::Device* pDevice);
    virtual ~SettingsLoader();

    virtual Result Init() override;

    const Gfx6PalSettings& GetSettings() const { return m_settings; }
    void ValidateSettings(PalSettings* pSettings);
    void OverrideDefaults(PalSettings* pSettings);

protected:
    Pal::Device* Device() { return static_cast<Pal::Device*>(m_pDevice); }

private:
    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);

    virtual void GenerateSettingHash() override;

    // Private members
    Pal::Device*    m_pDevice;
    Gfx6PalSettings m_settings;  ///< Gfx6 settings pointer

    // auto-generated functions
    virtual void SetupDefaults() override;
    virtual void ReadSettings() override;
    virtual void InitSettingsInfo() override;
    virtual void DevDriverRegister() override;

    const char* m_pComponentName = "Gfx6_Pal";
};

} // Gfx6
} // Pal
