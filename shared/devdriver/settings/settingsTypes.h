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

    using SettingsType = SettingsURIService::SettingType;
    /// This struct holds a pointer to a setting value and its associated type
    /// and size.
    struct SettingsValueRef
    {
        /// The type of the setting pointed to.
        SettingsType type;
        /// The size of the value pointed to by `pValue`.
        /// NOTE, for string settings, only fixed-size char array is supported.
        /// `size` represents the length of the array, and NOT the length of
        /// the string.
        uint32_t     size;
        /// A pointer to the setting value.
        void*        pValue;
    };
}

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
} // namespace SettingsRpcService
