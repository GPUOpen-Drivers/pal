/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palSettingsFileMgr.h"
#include "palDbgPrint.h"
#include "palListImpl.h"
#include <string.h>
#include <ctype.h>

namespace Util
{

// =====================================================================================================================
template <typename Allocator>
SettingsFileMgr<Allocator>::~SettingsFileMgr()
{
    // Clean up the settings list
    auto i = m_settingsList.Begin();
    while (i.Get() != nullptr)
    {
        m_settingsList.Erase(&i);
    }
    PAL_ASSERT(m_settingsList.NumElements() == 0);
}

// =====================================================================================================================
// Initialises the settings manager by finding and opening the settings file and reading all settings in the file
template <typename Allocator>
Result SettingsFileMgr<Allocator>::Init(
    const char* pSettingsPath)
{
    Result ret = Result::Success;

    // Open the settings file, if it exists
    char fileName[512];
    Snprintf(&fileName[0], sizeof(fileName), "%s/%s", pSettingsPath, m_pSettingsFileName);
    if (File::Exists(&fileName[0]) == false)
    {
        ret = Result::ErrorUnavailable;
    }
    else
    {
        // Open the config file for read-only access
        ret = m_settingsFile.Open(&fileName[0], FileAccessRead);
    }

    if (ret == Result::Success)
    {
        // Read the settings file one line at a time
        char currLine[256];
        size_t lineLength = 0;
        Result readResult = Result::Success;
        // An error is returned from ReadLine when EOF is encountered, so we can just loop until
        // we see an error of some kind
        while (readResult == Result::Success)
        {
            // Read the line, leaving space to add the terminating NULL char
            readResult = m_settingsFile.ReadLine(&currLine[0],
                                                 sizeof(currLine) - 1,
                                                 &lineLength);
            currLine[lineLength] = '\0';

            if ((readResult == Result::Success) && (lineLength > 0))
            {
                // Now parse the line
                uint32 idx = 0;
                // ignore leading whitespace
                while (isspace(static_cast<uint8>(currLine[idx])))
                {
                    idx++;
                }

                // ignore comment/empty lines
                if ((currLine[idx] == ';') || (currLine[idx] == '\0'))
                {
                    continue;
                }

                // all other lines are key, value pairs. Split at the comma and add them to our map
                char* pBuffer = NULL;
                char* pToken = Strtok(currLine, ", ", &pBuffer);
                if ((pToken != nullptr) && (strlen(pToken) > 0))
                {
                    uint32 hashedName = 0;
                    // If the first character of the string is a # that indicates that the name
                    // string is an already hashed setting name. In that case just convert to UINT32
                    if (pToken[0] == '#')
                    {
                        StringToValueType(&pToken[1], ValueType::Uint,
                                          sizeof(uint32), &hashedName);
                    }
                    else
                    {
                        // Otherwise, calculate the hashed value for the setting name
                        hashedName = HashString(pToken, strlen(pToken));
                    }

                    pToken = Strtok(nullptr, ",", &pBuffer);
                    if (pToken != nullptr)
                    {
                        // ignore leading whitespace
                        while ((*pToken != '\0') && isspace(static_cast<uint8>(*pToken)))
                        {
                            pToken++;
                        }

                        if (strlen(pToken) > 0)
                        {
                            SettingValuePair pair = { hashedName, {0} };
                            PAL_ASSERT(strlen(pToken) < sizeof(pair.strValue));
                            strncpy(&pair.strValue[0], pToken, sizeof(pair.strValue));
                            m_settingsList.PushBack(pair);
                        }
                    }
                }
            }
        }
        m_settingsFile.Close();
    }

    return ret;
}

// =====================================================================================================================
// Gets a setting's value based on a string value name
template <typename Allocator>
bool SettingsFileMgr<Allocator>::GetValue(
    const char* pValueName,
    ValueType   type,
    void*       pValue,
    size_t      bufferSz
    ) const
{
    // If the first character of the string is a # that indicates that the name strings is the
    // already hashed setting name in string form. In that case just convert to UINT32
    uint32 hashedName = 0;
    if (pValueName[0] == '#')
    {
        StringToValueType(&pValueName[1], ValueType::Uint, sizeof(uint32), &hashedName);
    }
    else
    {
        // Otherwise, calculate the hashed value for the setting name
        hashedName = HashString(pValueName, strlen(pValueName));
    }

    return GetValueByHash(hashedName, type, pValue, bufferSz);
}

// =====================================================================================================================
// Gets a setting's value based on a hashed value name
template <typename Allocator>
bool SettingsFileMgr<Allocator>::GetValueByHash(
    uint32    hashedName,
    ValueType type,
    void*     pValue,
    size_t    bufferSz
    ) const
{
    bool foundValue = false;

    // Get the value from the list
    auto iterator = m_settingsList.Begin();
    const char* pSettingValue = nullptr;
    while(iterator.Get() != nullptr)
    {
        if (iterator.Get()->hashName == hashedName)
        {
            pSettingValue = &iterator.Get()->strValue[0];
            break;
        }
        iterator.Next();
    }

    if(pSettingValue != nullptr)
    {
        // Indicate we found the value being requested
        foundValue = true;
        // Then convert it to the correct type and return
        StringToValueType(pSettingValue, type, bufferSz, pValue);
    }

    return foundValue;
}

} // Util
