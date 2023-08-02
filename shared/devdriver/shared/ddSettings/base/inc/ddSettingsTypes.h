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

// TODO: remove this once when we remove from ddSettingsServiceTypes.h
#define GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION 42

#include "../../../apis/settings/inc/dd_settings_api.h"
#include <ddPlatform.h>
#include "protocols/ddSettingsServiceTypes.h"

namespace DevDriver
{

static constexpr size_t kMaxComponentNameStrLen    = 64;
static constexpr size_t kSettingsMaxPathStrLen     = 512;
static constexpr size_t kSettingsMaxFileNameStrLen = 256;
static constexpr size_t kSettingsMaxMiscStrLen     = 128;

/// The maximum size for a Setting Value. This accounts for the possible string lengths.
constexpr size_t MaxSettingValueSize()
{
    using namespace Platform;
    return Max(kSettingsMaxPathStrLen, Max(kSettingsMaxFileNameStrLen, kSettingsMaxMiscStrLen));
}

using SettingsURIType = SettingsURIService::SettingType;

/// @deprecated This struct holds a pointer to a setting value and its associated type
/// and size.
struct SettingsValueRef
{
    /// The type of the setting pointed to.
    SettingsURIType type;
    /// The size of the value pointed to by `pValue`.
    /// NOTE, for string settings, only fixed-size char array is supported.
    /// `size` represents the length of the array, and NOT the length of
    /// the string.
    uint32_t size;
    /// A pointer to the setting value. The lifetime of the setting value
    /// is managed by where the data is stored (usually `SettingsBase`).
    void* pValue;
};

} // namespace DevDriver

namespace SettingsRpcService
{

/// The value buffer is sized to store the setting value and its maximum size.
constexpr size_t kSettingValueBufferSize = DevDriver::MaxSettingValueSize() +
    sizeof(DevDriver::SettingsURIService::SettingValue);

/// Structure used when calling SetData with RPC.
struct DDRpcSetDataInfo
{
    /// Name of the component
    char componentName[DevDriver::kMaxComponentNameStrLen];
    /// The setting's name hash
    DevDriver::SettingsURIService::SettingNameHash nameHash;
    /// Setting type
    uint32_t type;
    /// Setting Data
    uint8_t dataBuffer[kSettingValueBufferSize];
    /// Size of the setting data
    uint32_t dataSize;
};

/// This struct stores a pair of setting name hash and setting value, used for
/// Settings RPC call `GetCurrentValues`.
struct SettingsHashValuePair
{
    /// hash value of setting name
    DD_SETTINGS_NAME_HASH hash;
    /// setting type
    DD_SETTINGS_TYPE type;
    /// If multiple objects of this struct are stored in a continuous memory
    /// block, `nextOffset` points to the address of the next object in the
    /// memory.
    uint32_t nextOffset;
    /// The size of the buffer containing the value.
    uint32_t valueBufSize;
    /// A variable-sized array containing the value.
    uint8_t valueBuf[1];
};

/// This struct is a header that sits before `SettingsHashValuePair` in a
/// continuous memory block.
struct SettingsComponentValues
{
    /// The name of the component
    char componentName[DevDriver::kMaxComponentNameStrLen];
    /// The hash of the component's YAML data.
    uint64_t componentHash;
    /// The offset to the address of the next `SettingsComponentValues` in a
    /// continuous memory block.
    uint32_t nextOffset;
};

/// This struct stores parameters needed to make the `SetValue` RPC call.
struct SettingsSetValueRequestParams
{
    /// The name of the component from which a setting's value is to be set.
    char componentName[DevDriver::kMaxComponentNameStrLen];
    /// The setting's hash and the new value.
    SettingsHashValuePair hashValPair;
};

} // namespace SettingsRpcService
