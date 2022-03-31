/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <ddApi.h>
#include <util/hashMap.h>
#include <util/vector.h>
#include <protocols/ddSettingsServiceTypes.h>

namespace DevDriver
{

/// The Settings config is a file on local disk, that holds Settings user
/// values. The file must conform to the "Settings User Values Export/Import
/// Schema". This class helps load the user values from a Settings config file
/// and use them to override the existing values in a `SettingsBase` class.
class SettingsConfig
{
    using SettingNameHash = SettingsURIService::SettingNameHash;
    using SettingValue = SettingsURIService::SettingValue;
    using SettingType = SettingsURIService::SettingType;

// ========================================================================
// Member Data
private:
    // buffer of raw JSON text.
    Vector<char> m_buffer;
    // A pointer to the JSON document read from a local file. A void pointer is
    // used to hide JSON parser implementation, and prevent header dependency
    // propagation.
    void* m_pJson;
    bool m_validData;

public:
    SettingsConfig();
    ~SettingsConfig();

    DD_RESULT Load(const char* pFilePath);

    DD_RESULT ApplyUserValuesByComponent(
        const char*   pComponentName,
        void*         pSettingsBase
    );

    DD_RESULT ApplyUserValueByName(
        const char*   pSettingName,
        const char*   pComponentName,
        void*         pSettingsBase
    );

private:
    DD_DISALLOW_COPY_AND_ASSIGN(SettingsConfig);
};

} // namespace DevDriver
