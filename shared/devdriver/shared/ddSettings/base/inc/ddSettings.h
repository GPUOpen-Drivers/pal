/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t DD_SETTINGS_NAME_HASH;

typedef enum DD_SETTINGS_TYPE
{
    DD_SETTINGS_TYPE_BOOL = 0,
    DD_SETTINGS_TYPE_INT8,
    DD_SETTINGS_TYPE_UINT8,
    DD_SETTINGS_TYPE_INT16,
    DD_SETTINGS_TYPE_UINT16,
    DD_SETTINGS_TYPE_INT32,
    DD_SETTINGS_TYPE_UINT32,
    DD_SETTINGS_TYPE_INT64,
    DD_SETTINGS_TYPE_UINT64,
    DD_SETTINGS_TYPE_FLOAT,
    DD_SETTINGS_TYPE_STRING,
} DD_SETTINGS_TYPE;

typedef struct DDSettingsValueRef
{
    /// The hash value of the setting name.
    DD_SETTINGS_NAME_HASH hash;
    /// The type of the setting.
    DD_SETTINGS_TYPE type;
    /// The size of the value pointed to by `pValue`. NOTE, for string
    /// settings, only fixed-size char array is supported. `size` represents
    /// the length of the array, and NOT the length of the string.
    uint32_t size;
    /// A pointer to the setting value.
    void* pVal;
} DDSettingsValueRef;

#ifdef __cplusplus
} // extern "C"
#endif
