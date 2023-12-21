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
#include <dd_settings_rpc_types.h>

namespace DevDriver
{

/// This class helps users iterate through settings components and their values packed in a raw buffer.
class SettingsIterator
{
public:
    struct Component
    {
        // A pointer to null-terminated component name string.
        const char* pName;

        // Hash value of the settings JSON blob of this component.
        uint64_t    blobHash;

        // The number of settings values in this component.
        uint16_t    numValues;

        // An opaque offset representing a settings component. Users must not modify this value.
        size_t      offset;

        Component()
            : pName{nullptr}
            , numValues{0}
            , offset{0}
        {}
    };

    struct Value
    {
        DDSettingsValueRef valueRef;

        // An opaque offset representing a settings component. Users must not modify this value.
        size_t offset;

        Value()
            : valueRef{}
            , offset{0}
        {}
    };

private:
    const uint8_t* m_pBuf;
    size_t         m_bufSize;

    DDSettingsAllComponentsHeader m_allComponentsHeader;

    DD_RESULT m_error;

public:
    /// @param pBuf A pointer to a buffer holding settings data.
    /// @param size The size of the buffer.
    SettingsIterator(const uint8_t* pBuf, size_t size);

    ~SettingsIterator() = default;

    /// Get the next component in the settings data.
    ///
    /// @param[in,out] pComponent A pointer to an existing \ref SettingsIterator.Component to receive the next
    /// component data. To get the first component, the pointed to object must be zero-initialized.
    /// @return true if a valid component is found, false otherwise.
    bool NextComponent(Component* pComponent);

    /// Get the next setting value of the current component in the settings data.
    ///
    /// @param[in,out] pValue A pointer to an existing \ref SettingsIterator.Value to receive the next value data.
    /// To get the first value, the pointed to object must be zero-initialized.
    /// @return true if a valid value is found, false otherwise.
    bool NextValue(const Component& component, Value* pValue);
};

} // namespace DevDriver
