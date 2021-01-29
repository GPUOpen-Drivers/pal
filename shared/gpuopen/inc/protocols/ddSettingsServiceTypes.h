/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  ddSettingsService.h
* @brief Class declaration for the Settings URI Service.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
namespace SettingsURIService
{

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// The following data is defined by the settings service and will be used by the client as part of some requests

DD_STATIC_CONST uint32 kMaxComponentNameStrLen = 64;
// Set a reasonable max size limit for setting value of 1MB
DD_STATIC_CONST uint32 kMaxSettingValueSize = 0x100000;

// Key name for the JSON response to the components command
DD_STATIC_CONST const char* Components_ComponentsKey = "components";

// We define the basic data types in this struct, components are free to extend this enumeration to include
// additional/custom types.
enum struct SettingType : uint32
{
    Boolean = 0,
    Int     = 1,
    Uint    = 2,
    Int64   = 3,
    Uint64  = 4,
    Float   = 5,
    String  = 6,
};

// Struct to hold a setting value
struct SettingValue
{
    SettingType type;
    void*       pValuePtr;
    size_t      valueSize;
};

// Header for the settingsData command
struct SettingsDataHeader
{
    bool   isEncoded;           //< Indicates if the settings data is in plain text JSON or encoded.
    uint32 magicBufferId;       //< ID for the file used for decoding JSON data.
    uint32 magicBufferOffset;   //< Offset within the magic buffer file to start at when decoding.
};

// The hash type is intentionally opaque to allow individual components to use whatever hashing method they like.
typedef uint32 SettingNameHash;

// End of shared data
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// GetData is expected to copy the setting value into the provided SettingValue struct. In cases where pValuePtr
// must be used and valueSize is too small to fit the data, the function will return Result::InsufficientMemory
// updating valueSize with the required size.  Otherwise it will return Success on successful copy of the
// setting value.  valueSize should be set to 0 when the value union holds the setting value data.
typedef DevDriver::Result (*SettingGetValueFunc)(
    SettingNameHash hash,
    SettingValue*   pSettingValue,
    void*           pPrivateData);

// SetData provides a new value for the setting corresponding to the indicated name hash. This function may
// return SettingsUriInvalidSettingValue if the data provided does not match the setting type, size or other requirements.
typedef DevDriver::Result (*SettingSetValueFunc)(
    SettingNameHash     hash,
    const SettingValue& settingValue,
    void*               pPrivateData);

// This struct contains the data necessary to register a component in the Settings service. A component is a set of
// settings related in some way. The definition of a component is intentionally loose to allow the driver to divide
// its settings into separate groups as necessary.
struct RegisteredComponent
{
    char                   componentName[kMaxComponentNameStrLen];  //< Component name string
    const SettingNameHash* pSettingsHashes;                         //< Array of valid setting hashes
    uint32                 numSettings;                             //< Number of setting hashes in pSettingsHashes
    SettingGetValueFunc    pfnGetValue;                             //< Function called to get a setting value
    SettingSetValueFunc    pfnSetValue;                             //< Function called to set a setting value
    SettingsDataHeader     settingsDataHeader;                      //< Info about how JSON data is encoded
    const void*            pSettingsData;                           //< Full settings JSON data
    size_t                 settingsDataSize;                        //< Size of full settings data blob
    uint64                 settingsDataHash;                        //< Hash of the settings data
    void*                  pPrivateData;                            //< Private context data that will be sent back to the
                                                                    //  component when Get/SetValue functions are called.
};
} // SettingsURIService
} // DevDriver
