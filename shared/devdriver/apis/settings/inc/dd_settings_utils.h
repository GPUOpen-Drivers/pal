/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include <string>
#include <vector>

namespace DevDriver
{
namespace SettingsUtils
{

struct SettingValue
{
    bool operator==(const SettingValue& other) const
    {
        return (numVal.all == other.numVal.all) && (strVal == other.strVal) && (isOptional == other.isOptional);
    }

    union
    {
        bool  b;
        float f;

        int8_t  i8;
        int16_t i16;
        int32_t i32;
        int64_t i64;

        uint8_t  u8;
        uint16_t u16;
        uint32_t u32;
        uint64_t u64;

        uint64_t all;

    } numVal;  // The numerical value of the setting.

    std::string strVal;  // The string value of the setting.
    bool isOptional;
};

// @ToDo: Populate valid values, tags, etc as needed
struct SettingsData
{
    std::string           name;
    std::string           description;
    std::string           structName; // Only valid if it is part of a struct
    DD_SETTINGS_NAME_HASH nameHash;
    DD_SETTINGS_TYPE      type;
    SettingValue          value;
};

struct SettingComponent
{
    std::string               name;
    std::vector<SettingsData> settings;
};

DD_RESULT ParseSettingsBlobs(const char* pBlobBuffer, size_t bufferSize, std::vector<SettingComponent>& output);

} // SettingsUtils namespace

} // DevDriver namespace
