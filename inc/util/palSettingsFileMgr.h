/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
 ***********************************************************************************************************************
 * @file  palSettingsFileMgr.h
 * @brief PAL utility collection SettingsFileMgr class declaration.
 ***********************************************************************************************************************
 */

#pragma once

#include "palFile.h"
#include "palInlineFuncs.h"
#include "palList.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief Parses a plain text config file filled with key/value pairs that describe user-desired driver settings.  The
 *        format of the target file should look like this:
 *
 *        ; Comment
 *        SettingName, StringValue
 *        AnotherSettingName, 1234
 *
 *        ; The following settings are pre-hashed.
 *        #0x9370a0c8, AnotherStringValue
 *
 *        After loading the file, a value can be retrieved by either specifying a setting string or hash value.
 ***********************************************************************************************************************
 */
template <typename Allocator>
class SettingsFileMgr
{
public:
    /// Constructs a settings file manager object that will read driver settings from the specified file.
    ///
    /// @param [in] pSettingsFileName Name of settings file.
    /// @param [in] pAllocator        Class which provides allocation functions for this manager.
    SettingsFileMgr(const char* pSettingsFileName, Allocator*const pAllocator)
        :
        m_pSettingsFileName(pSettingsFileName),
        m_settingsList(pAllocator)
    {
    }

    /// Destroys the object and closes the associated file if it is still open.
    ~SettingsFileMgr();

    /// Initializes the settings file manager.  Must be called before calling any other functions on this object.
    ///
    /// @param [in] pSettingsPath The path to to the settings file.
    ///
    /// @returns Success if reading the file is successful. Other appropriate error codes are returned otherwise.
    Result Init(const char* pSettingsPath);

    /// Returns the value corresponding to the specified setting in this settings file.
    ///
    /// @param [in]  pSettingName C-style string specifying the setting name to get.
    /// @param [in]  type         Expected value type (e.g., int, float, string).  Specifies how the value specified
    ///                           in the file should be converted to the data written into pValue.
    /// @param [out] pValue       Value as read from the settings file and converted to the specified type.
    /// @param [in]  bufferSz     If type is ValueType::Str, this value indicates the amount of available space at
    ///                           pValue.  If not enough space is available, the resulting string value will be
    ///                           truncated.
    ///
    /// @returns True if the value was successfully found in the settings file and converted to the requested value
    ///          type; false otherwise.
    bool GetValue(const char* pSettingName, ValueType type, void* pValue, size_t bufferSz = 0) const;

    /// Returns the value corresponding to the specified setting in this settings file.  This method accepts a
    /// pre-hashed settings string using the HashString() function defined in palInlineFuncs.h.
    ///
    /// @param [in]  hashedName 32-bit hash result from calling HashString() on the setting name of interest.
    /// @param [in]  type       Expected value type (e.g., int, float, string).  Specifies how the value specified
    ///                         in the file should be converted to the data written into pValue.
    /// @param [out] pValue     Value as read from the settings file and converted to the specified type.
    /// @param [in]  bufferSz   If type is ValueType::Str, this value indicates the amount of available space at
    ///                         pValue.  If not enough space is available, the resulting string value will be
    ///                         truncated.
    ///
    /// @returns True if the value was successfully found in the settings file and converted to the requested value
    ///          type; false otherwise.
    bool GetValueByHash(uint32 hashedName, ValueType type, void* pValue, size_t bufferSz = 0) const;

private:
    // Describes a single { setting, value } pair as loaded from a settings file.
    struct SettingValuePair
    {
        uint32 hashName;      // 32-bit hash of the setting name string.
        char   strValue[512];  // Value for this setting encoded as a C-stlye string.
    };

    const char*const m_pSettingsFileName;
    File             m_settingsFile;

    // List of setting, value pairs parsed from the config file.
    List<SettingValuePair, Allocator> m_settingsList;

    PAL_DISALLOW_COPY_AND_ASSIGN(SettingsFileMgr);
};

} // Util
