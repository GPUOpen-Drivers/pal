/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "gpuopen.h"
#include "util/ddJsonWriter.h"

using namespace DevDriver;
using namespace rapidjson;

//=====================================================================================================================
static Result Bool2Result(bool predicate)
{
    return predicate ? Result::Success : Result::Error;
}

//=====================================================================================================================
Result JsonWriter::End()
{
    if (m_lastResult == Result::Success)
    {
        m_lastResult = Bool2Result(m_rjWriter.IsComplete());
    }

    const Result jsonResult = m_lastResult;
    // Note: It is important to call m_textStream.End() here. This NUL terminates the stream
    //       and makes it easier to debug partially written json - especially if the error is bad json usage.
    //       TextWriter::End() will behavior correctly on its own if there's already a TextWriter error.
    const Result textResult = m_textStream.End();

    // We can only return one error, so we need to prioritize these.
    // Json errors come from RapidJSON and represent programmer or OoM errors.
    // TextWriter errors come from the user's callback and could be anything.
    //     These errors may be more severe, or something the user has more control over.
    // Therefore, we make the choice to mask json errors if there are text errors too.
    Result result = jsonResult;
    if (textResult != Result::Success)
    {
        result = textResult;
    }

    // Overwrite the last result with success to allow for subsequent uses of the writer.
    m_lastResult = Result::Success;

    // Reset the writer stream before finishing the response.
    m_rjWriter.Reset(m_textStream);

    return result;
}

// ===== Collection Writers ===========================================================================================

//=====================================================================================================================
void JsonWriter::BeginList()
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.StartArray());
    }
}

//=====================================================================================================================
void JsonWriter::EndList()
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.EndArray());
    }
}

//=====================================================================================================================
void JsonWriter::BeginMap()
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.StartObject());
    }
}

//=====================================================================================================================
void JsonWriter::EndMap()
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.EndObject());
    }
}

//=====================================================================================================================
void JsonWriter::Key(const char* pKey)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Key(pKey));
    }
}

// ===== Value Writers ================================================================================================

//=====================================================================================================================
void JsonWriter::Value(const char* pValue)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.String(pValue));
    }
}

//=====================================================================================================================
void JsonWriter::Value(const char* pValue, size_t length)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.String(pValue, static_cast<rapidjson::SizeType>(length)));
    }
}

//=====================================================================================================================
void JsonWriter::Value(uint64 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint64(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(uint32 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(uint16 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(uint8 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(int64 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Int64(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(int32 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Int(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(int16 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(int8 value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Uint(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(double value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Double(value));
    }
}

//=====================================================================================================================
void JsonWriter::Value(float value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Double(static_cast<double>(value)));
    }
}

//=====================================================================================================================
void JsonWriter::Value(char value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.String(&value, 1, true));
    }
}

//=====================================================================================================================
void JsonWriter::Value(bool value)
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Bool(value));
    }
}

//=====================================================================================================================
void JsonWriter::ValueNull()
{
    if (CanWrite())
    {
        m_lastResult = Bool2Result(m_rjWriter.Null());
    }
}
