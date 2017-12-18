/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "core/settingsLoader.h"
#include "core/hw/gfxip/gfx6/g_gfx6PalSettings.h"

namespace Pal
{

class Device;

namespace Gfx6
{

// =====================================================================================================================
// This class is responsible for loading the Gfx6-specific portion of the PalSettings
// structure specified in the constructor.  This is a helper class that only exists for a short
// time while the settings are initialized.
class SettingsLoader : public Pal::SettingsLoader
{
public:
    SettingsLoader(Pal::Device* pDevice);

    static const Gfx6PalSettings* GetSettings(const PalSettings* pSettings)
        { return (static_cast<const Gfx6PalSettings*>(pSettings)); }

protected:
    virtual void HwlInit() override;
    virtual void HwlValidateSettings() override;
    virtual void HwlReadSettings() override;

private:
    virtual ~SettingsLoader();

    virtual void GenerateSettingHash() override;

    void OverrideGfx6Defaults();

    // auto-generated functions
    static void Gfx6SetupDefaults(Gfx6PalSettings* pSettings);
    void Gfx6ReadSettings(Gfx6PalSettings* pSettings);

    // Private members
    Gfx6PalSettings m_gfx6Settings;  ///< Gfx6 settings info

    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(SettingsLoader);
};

} // Gfx6
} // Pal
