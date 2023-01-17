/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "ddSettings.h"
#include <ddApi.h>
#include <ddDefs.h>

namespace DevDriver
{

struct SettingsUserOverride
{
    const char*           pName;
    size_t                nameLength;
    DD_SETTINGS_NAME_HASH nameHash;
    bool                  isValid;
    DD_SETTINGS_TYPE      type;
    uint32_t              size;
    union
    {
        bool        b;
        int8_t      i8;
        uint8_t     u8;
        int16_t     i16;
        uint16_t    u16;
        int32_t     i32;
        uint32_t    u32;
        int64_t     i64;
        uint64_t    u64;
        float       f;
        const char* s;
    } val;

    template<typename T>
    void SetVal(T value)
    {
        *((T*)&val) = value;
    }
};

/// Iterate through all user-overrides in a component by repeatedly
/// calling `SettingsUserOverrideIter::Next()` until
/// `SettingsUserOverrideIter::IsValid()` returns false. The lifetime
/// of this object is tied to that of SettingsConfig.
class SettingsUserOverrideIter
{
    friend class SettingsUserOverridesLoader;

private:
    // a pointer to `yaml_document_t`
    void* m_pDoc;
    // a pointer to `yaml_node_t`, representing a sequence node of user-overrides
    void* m_pUserOverridesNode;
    // a pointer to `yaml_node_item_t`, index of individual user-override node
    void* m_pUserOverrideNodeIndex;

public:
    bool IsValid()
    {
        return (m_pDoc != nullptr) &&
            (m_pUserOverridesNode != nullptr) &&
            (m_pUserOverrideNodeIndex != nullptr);
    }

    /// Get the next UserOverride in the component.
    SettingsUserOverride Next();
};

/// SettingsUserOverrides is a YAML file on local disk, that holds Settings
/// user overrides. The file must conform to the "Settings User Overrides
/// Schema". This class helps load the user overrides from a Settings
/// useroverrides file and use them to overwrite the existing values in a
/// `SettingsBase` class.
class SettingsUserOverridesLoader
{
// ========================================================================
// Member Data
private:
    char* m_pBuffer; // buffer of raw YAML text
    void* m_pParser; // a pointer to `yaml_parser_t`
    void* m_pDocument; // a pointer to `yaml_document_t`
    bool m_valid; // If loaded YAML data is valid.

public:
    SettingsUserOverridesLoader();
    ~SettingsUserOverridesLoader();

    /// Load and store the content of a Settings UserOverrides file. The
    /// file must conform to the second version of "Settings User Overrides
    /// Schema" described in settings_useroverrides_schema.yaml.
    DD_RESULT Load(const char* pFilePath);

    /// Return an iterator that retrives all user-overrides in a component.
    SettingsUserOverrideIter GetUserOverridesIter(const char* pComponentName);

    /// Return a user-override by its setting's name hash.
    SettingsUserOverride GetUserOverrideByNameHash(
        const char* pComponentName,
        DD_SETTINGS_NAME_HASH nameHash);

private:
    DD_DISALLOW_COPY_AND_ASSIGN(SettingsUserOverridesLoader);
};

} // namespace DevDriver
