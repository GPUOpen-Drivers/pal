/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "g_expSettings.h"
#include "palDevice.h"

namespace Pal
{
class Platform;

// =====================================================================================================================
// This class is responsible for loading the ExpSettings structure specified in the
// constructor.  This is a helper class that only exists for a short time while the settings
// are initialized.
class ExperimentsLoader : public DevDriver::SettingsBase
{
public:

    explicit ExperimentsLoader(Platform* pPlatform);
    virtual ~ExperimentsLoader();

    Result Init();

    const PalExperimentsSettings& GetSettings() const { return m_settings; }
    PalExperimentsSettings* GetSettingsPtr() { return &m_settings; }

    // Auto-generated
    uint64 GetSettingsBlobHash() const override;

private:

    // Auto-generated function
    virtual const char* GetComponentName() const override;
    virtual DD_RESULT SetupDefaultsAndPopulateMap() override;

    PAL_DISALLOW_COPY_AND_ASSIGN(ExperimentsLoader);
    PAL_DISALLOW_DEFAULT_CTOR(ExperimentsLoader);

    Platform*              m_pPlatform;
    PalExperimentsSettings m_settings;
};

} // Pal
