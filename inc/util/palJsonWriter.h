/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palUtil.h"
#include "palAssert.h"

namespace Util
{

/**
 ***********************************************************************************************************************
 * @brief An abstract class that defines the interface between the general JsonWriter and the client's specific output
 *        requirements.  For example, the client may wish to stream the JSON text to a file.
 ***********************************************************************************************************************
 */
class JsonStream
{
public:
    /// Destructor.
    virtual ~JsonStream() {}

    /// Called when the corresponding JsonWriter wishes to output a string.
    ///
    /// @param [in] pString The null-terminated string being written to the stream.
    /// @param [in] length  The length of the string not including the null terminator ("Hello" = 5).
    virtual void WriteString(const char* pString, uint32 length) = 0;

    /// Called when the corresponding JsonWriter wishes to output a single character.
    ///
    /// @param [in] character The character being written to the stream.
    virtual void WriteCharacter(char character) = 0;
};

/**
 ***********************************************************************************************************************
 * @brief Generates JSON text and writes it to the provided JsonStream.
 *
 * See http://www.json.org/ for a complete description of the JSON standard.  This class implements that standard with
 * a couple of exceptions.  First, "Object" and "Array" have been renamed to "Map" and "List" respectively.  Second,
 * this class makes no attempt to produce Unicode characters or escape control characters.
 *
 * The JsonWriter functions that program the JSON text stream do not return an error code if they are used incorrectly;
 * however, they will assert on debug builds.  The caller must understand the JSON standard and only instruct the
 * JsonWriter to write legal JSON.
 ***********************************************************************************************************************
 */
class JsonWriter
{
public:
    /// Constructor.
    ///
    /// @param [in] pStream The JsonWriter will use this stream to output all of its text.
    explicit JsonWriter(JsonStream* pStream);

    /// Destructor.
    virtual ~JsonWriter() {}

    /// Instructs the JsonWriter to begin writing a new list collection.
    ///
    /// @param [in] isInline If true, the JsonWriter will format the contents of this list as a single line.
    void BeginList(bool isInline);

    /// Instructs the JsonWriter to end the current list collection.
    void EndList();

    /// Instructs the JsonWriter to begin writing a new map collection.
    ///
    /// @param [in] isInline If true, the JsonWriter will format the contents of this map as a single line.
    void BeginMap(bool isInline);

    /// Instructs the JsonWriter to end the current map collection.
    void EndMap();

    /// Instructs the JsonWriter to write a key into a map.
    ///
    /// @param [in] pKey A null-terminated string naming the key.
    void Key(const char* pKey);

    /// Instructs the JsonWriter to write a string value.
    ///
    /// @param [in] pValue The null-terminated string to write.
    void Value(const char* pValue);

    ///@{
    /// Instructs the JsonWriter to write a value.
    ///
    /// @param [in] value The value to write.
    void Value(uint64 value);
    void Value(uint32 value);
    void Value(uint16 value);
    void Value(uint8 value);
    void Value(int64 value);
    void Value(int32 value);
    void Value(int16 value);
    void Value(int8 value);
    void Value(float value);
    void Value(bool value);
    ///@}

    /// Instructs the JsonWriter to write a JSON "null" value.
    void NullValue();

    /// Instructs the JsonWriter to write a key-value pair where the value will be a list.
    ///
    /// @param [in] pKey     A null-terminated string naming the key.
    /// @param [in] isInline If true, the JsonWriter will format the contents of the list as a single line.
    void KeyAndBeginList(const char* pKey, bool isInline) { Key(pKey); BeginList(isInline); }

    /// Instructs the JsonWriter to write a key-value pair where the value will be a map.
    ///
    /// @param [in] pKey     A null-terminated string naming the key.
    /// @param [in] isInline If true, the JsonWriter will format the contents of the map as a single line.
    void KeyAndBeginMap(const char* pKey, bool isInline) { Key(pKey); BeginMap(isInline); }

    /// Instructs the JsonWriter to write a key-value pair.
    ///
    /// @param [in] pKey   A null-terminated string naming the key.
    /// @param [in] pValue The null-terminated string value.
    void KeyAndValue(const char* pKey, const char* pValue) { Key(pKey); Value(pValue); }

    ///@{
    /// Instructs the JsonWriter to write a key-value pair.
    ///
    /// @param [in] pKey  A null-terminated string naming the key.
    /// @param [in] value The value to write.
    void KeyAndValue(const char* pKey, uint64 value) { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, uint32 value) { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, uint16 value) { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, uint8 value)  { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, int64 value)  { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, int32 value)  { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, int16 value)  { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, int8 value)   { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, float value)  { Key(pKey); Value(value); }
    void KeyAndValue(const char* pKey, bool value)   { Key(pKey); Value(value); }
    ///@}

    /// Instructs the JsonWriter to write a key-value pair where the value is a JSON "null" value.
    ///
    /// @param [in] pKey A null-terminated string naming the key.
    void KeyAndNullValue(const char* pKey) { Key(pKey); NullValue(); }

private:
    void MaybeNextListEntry();
    void TransitionToToken(uint32 nextToken, bool leavingScope);

#if PAL_ENABLE_PRINTS_ASSERTS
    bool ValidateTransition(uint32 nextToken);
#endif

    static constexpr uint32 ScopeStackSize = 32; ///< The maximum size of the scope stack, see m_scopeStack for details.
    static constexpr uint32 IndentSize     = 2;  ///< The number of space characters per scope indentation.

    JsonStream*const m_pStream;   ///< Used to reallocate m_pBuffer as needed.
    uint32           m_prevToken; ///< The last token that was written, used to determine what whitespace to write next.
    uint32           m_curScope;  ///< The writer is currently at this index in the scope stack.

    /// The scope stack tracks all active scopes so that the writer knows what kind of collection it is building after
    /// it completes its current collection. The first scope will always be ScopeOutside. For simplicity, no more than
    /// (ScopeStackSize - 1) layered collections are supported.
    uint8            m_scopeStack[ScopeStackSize];

    /// This buffer holds enough space characters to indent out to a full scope stack.
    char             m_indentBuffer[ScopeStackSize * IndentSize];

    PAL_DISALLOW_DEFAULT_CTOR(JsonWriter);
    PAL_DISALLOW_COPY_AND_ASSIGN(JsonWriter);
};

} // Util
