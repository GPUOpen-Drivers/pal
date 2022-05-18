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

#include "settingsParsingUtils.h"

using RjValue = rapidjson::Value;

// ============================================================================
DD_RESULT RjGetMemberString(const RjValue* json, const char* jsonField, const char** ppOutStr)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *ppOutStr = nullptr;

    RjValue::ConstMemberIterator str = json->FindMember(jsonField);
    if ((str != json->MemberEnd()) && str->value.IsString())
    {
        *ppOutStr = str->value.GetString();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberUint32(const RjValue* json, const char* jsonField, uint32_t* pOutInt)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutInt = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsUint())
    {
        *pOutInt = integer->value.GetUint();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberInt32(const RjValue* json, const char* jsonField, int32_t* pOutInt)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutInt = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsInt())
    {
        *pOutInt = integer->value.GetInt();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberUint64(const RjValue* json, const char* jsonField, uint64_t* pOutInt)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutInt = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsUint64())
    {
        *pOutInt = integer->value.GetUint64();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberInt64(const RjValue* json, const char* jsonField, int64_t* pOutInt)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutInt = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsInt64())
    {
        *pOutInt = integer->value.GetInt64();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberBool(const RjValue* json, const char* jsonField, bool* pOutBool)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutBool = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsBool())
    {
        *pOutBool = integer->value.GetBool();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}

// ============================================================================
DD_RESULT RjGetMemberFloat(const RjValue* json, const char* jsonField, float* pOutFloat)
{
    DD_RESULT result = DD_RESULT_SUCCESS;
    *pOutFloat = 0;

    RjValue::ConstMemberIterator integer = json->FindMember(jsonField);
    if ((integer != json->MemberEnd()) && integer->value.IsFloat())
    {
        *pOutFloat = integer->value.GetFloat();
    }
    else
    {
        result = DD_RESULT_PARSING_INVALID_JSON;
    }

    return result;
}
