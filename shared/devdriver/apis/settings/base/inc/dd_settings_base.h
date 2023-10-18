/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_settings_api.h>

#include <util/hashMap.h>

#include <cstdint>

namespace Pal
{
class Device;
} // namespace Pal

class DdiAdapter;

namespace DevDriver
{

using SettingsHashMap = HashMap<DD_SETTINGS_NAME_HASH, DDSettingsValueRef>;

/// The base struct for storing settings data. Subclasses of different Settings
/// components are auto-generated based on settings YAML files.
struct SettingsDataBase
{
    /// Number of total settings.
    uint32_t numSettings;
};

/// The base class for Settings components.
class SettingsBase
{
private:
    void* const m_pSettingsData;

protected:
    SettingsHashMap m_settingsMap;

public:
    SettingsBase(void* pSettingsData, size_t settingsDataSize);

    virtual ~SettingsBase();

    virtual const char* GetComponentName() const = 0;

    /// Set the value of a setting.
    DD_RESULT SetValue(const DDSettingsValueRef& srcValueRef);

    /// Get the value of a setting.
    /// `pValueRef` is an in/out parameter.
    /// `pValueRef->hash`   [in] The hash of the setting to be retrieved.
    /// `pValueRef->pValue` [in/out] The pointer to a pre-allocated buffer that the setting value will be copied to.
    /// `pValueRef->size`   [in] The size of the buffer `pValueRef->pValue` points to.
    /// `pValueRef->type`   [out] The type of the setting when this function succeeds.
    DD_RESULT GetValue(DDSettingsValueRef* pValueRef);

    /// Returns a 32-bit hash of an input string using the FNV-1a non-cryptographic hash function.
    /// `pStr` a pointer to the input string.
    /// 'strSize` the size of the input string, not including null-terminator
    static constexpr uint32_t Fnv1aCompTime(const char* const pStr, size_t strSize)
    {
        // Both `prime` and `hash` must match the ones used in settings_codegen.py.
        constexpr uint32_t prime = 0x01000193;
        uint32_t           hash = 0x811C9DC5;
        for (size_t i = 0; i < strSize; ++i)
        {
            hash = (hash ^ pStr[i]) * prime;
        }
        return hash;
    }

protected:
    /// This function is called in `SetValue()` before actually setting the
    /// value, giving derived classes a chance to intercept and perform
    /// custom actions. If this function returns true, `SetValue` will stop
    /// setting the value. Otherwise, `SetValue` sets the value as usual via memcpy.
    virtual bool CustomSetValue(const DDSettingsValueRef& valueRef)
    {
        (void)valueRef;
        return false;
    }

    /// Auto-generated. Set default setting values, and populate `m_pSettingsMap`.
    virtual DD_RESULT SetupDefaultsAndPopulateMap() = 0;
    /// Auto-generated Function signature for PAL related Settings..
    virtual void ReadSettings(Pal::Device*) {}
    /// Auto-generated. Function signature for DXC related Settings.
    virtual void ReadSettings(DdiAdapter*) {}

private:
    SettingsBase() = delete;
    SettingsBase(const SettingsBase&) = delete;
    SettingsBase& operator=(const SettingsBase&) = delete;
};

} // namespace DevDriver
