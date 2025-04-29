/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma pack(push, 1)
struct DDSettingsAllComponentsHeader
{
    uint16_t version;
    uint16_t numComponents;
};

struct DDSettingsComponentHeader
{
    // The name of the component, null terminated.
    char name[DD_SETTINGS_MAX_COMPONENT_NAME_SIZE];
    // The hash value of the JSON blob of this component.
    uint64_t blobHash;
    // The number of values in the component.
    uint16_t numValues;
    // The size of this header plus the size of all values immediately following this header.
    uint32_t size;
};

struct DDSettingsValueHeader
{
    DD_SETTINGS_NAME_HASH hash;
    uint8_t type;  // DD_SETTINGS_TYPE
    uint16_t valueSize; // The size of value data immediately following this header.
};

struct DDSettingsSiphonQuerySettingsBlobsAllParams
{
    /// Client driver type.
    DD_SETTINGS_DRIVER_TYPE driverType;

    /// Whether to reload settings blobs or use the cached data.
    bool reload;

    /// The size of the absolute path of driver to override, including null-terminator. If 0, the
    /// default path is used.
    uint16_t driverPathOverrideSize;
};
#pragma pack(pop)

static_assert(sizeof(DDSettingsAllComponentsHeader) == 4, "Unexpected size for DDSettingsAllComponentsHeader.");

static_assert(
    sizeof(DDSettingsComponentHeader) == DD_SETTINGS_MAX_COMPONENT_NAME_SIZE + 14,
    "Unexpected size for DDSettingsComponentHeader.");

static_assert(sizeof(DDSettingsValueHeader) == 7, "Unexpected size for DDSettingsValueHeader.");
