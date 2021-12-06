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

#include "palAssert.h"
#include "palInlineFuncs.h"
#include "palJsonWriter.h"
#include <cinttypes>

namespace Util
{

// Represents a logical token that can appear in the JSON text; some may be composed of many characters.
enum JsonToken : uint32
{
    TokenNone = 0,
    TokenLBrace,   // {
    TokenRBrace,   // }
    TokenLBracket, // [
    TokenRBracket, // ]
    TokenComma,    // ,
    TokenKey,      // string:
    TokenValue,    // string or number
    TokenCount
};

// Used to describe the properties of the current scope of the JSON writer. For example, these can be used to
// distinguish a multi-line map collection (ScopeMap) from an inline list collection (ScopeList | ScopeInline).
enum JsonScope : uint8
{
    ScopeOutside = 0x1,
    ScopeList    = 0x2,
    ScopeMap     = 0x4,
    ScopeInline  = 0x8
};

// =====================================================================================================================
JsonWriter::JsonWriter(
    JsonStream* pStream)
    :
    m_pStream(pStream),
    m_prevToken(TokenNone),
    m_curScope(0)
{
    PAL_ASSERT(m_pStream != nullptr);

    memset(m_scopeStack,   0,   sizeof(m_scopeStack));
    memset(m_indentBuffer, ' ', sizeof(m_indentBuffer));

    m_scopeStack[0] = ScopeOutside;
}

// =====================================================================================================================
void JsonWriter::BeginList(
    bool isInline)
{
    MaybeNextListEntry();
    TransitionToToken(TokenLBracket, false);
    m_pStream->WriteCharacter('[');

    // Add a new scope for this list.
    PAL_ASSERT(m_curScope + 1 < ScopeStackSize);
    m_scopeStack[++m_curScope] = isInline ? (ScopeList | ScopeInline) : ScopeList;
}

// =====================================================================================================================
void JsonWriter::EndList()
{
    TransitionToToken(TokenRBracket, true);
    m_pStream->WriteCharacter(']');

    // Exit this 's scope.
    PAL_ASSERT(m_curScope > 0);
    m_curScope--;
}

// =====================================================================================================================
void JsonWriter::BeginMap(
    bool isInline)
{
    MaybeNextListEntry();
    TransitionToToken(TokenLBrace, false);
    m_pStream->WriteCharacter('{');

    // Add a new scope for this map.
    PAL_ASSERT(m_curScope + 1 < ScopeStackSize);
    m_scopeStack[++m_curScope] = isInline ? (ScopeMap | ScopeInline) : ScopeMap;
}

// =====================================================================================================================
void JsonWriter::EndMap()
{
    TransitionToToken(TokenRBrace, true);
    m_pStream->WriteCharacter('}');

    // Exit this map's scope.
    PAL_ASSERT(m_curScope > 0);
    m_curScope--;
}

// =====================================================================================================================
void JsonWriter::Key(
    const char* pKey)
{
    // If we're in a map but not at the beginning, we require a comma token.
    if (TestAnyFlagSet(m_scopeStack[m_curScope], ScopeMap) && (m_prevToken != TokenLBrace))
    {
        TransitionToToken(TokenComma, false);
        m_pStream->WriteCharacter(',');
    }

    TransitionToToken(TokenKey, false);
    m_pStream->WriteCharacter('"');
    m_pStream->WriteString(pKey, static_cast<uint32>(strlen(pKey)));
    m_pStream->WriteCharacter('"');
    m_pStream->WriteCharacter(':');
}

// =====================================================================================================================
void JsonWriter::Value(
    const char* pValue)
{
    MaybeNextListEntry();
    TransitionToToken(TokenValue, false);
    m_pStream->WriteCharacter('"');
    m_pStream->WriteString(pValue, static_cast<uint32>(strlen(pValue)));
    m_pStream->WriteCharacter('"');
}

// =====================================================================================================================
template <typename T>
void JsonWriter::FormattedValue(
    const char* pFormat,
    T value)
{
    MaybeNextListEntry();
    TransitionToToken(TokenValue, false);

    constexpr size_t BufferSize = 32;
    char             buffer[BufferSize];
    const int        length = Snprintf(buffer, BufferSize, pFormat, value);

    PAL_ASSERT((length >= 0) && (length <= static_cast<int>(BufferSize)));

    m_pStream->WriteString(buffer, static_cast<uint32>(length));
}

// =====================================================================================================================
void JsonWriter::HexValue(uint64 value) { FormattedValue("\"0x%016\"" PRIx64, value); }
void JsonWriter::HexValue(uint32 value) { FormattedValue("\"0x%08\""  PRIx32, value); }
void JsonWriter::HexValue(uint16 value) { FormattedValue("\"0x%04\""  PRIx16, value); }
void JsonWriter::HexValue(uint8  value) { FormattedValue("\"0x%02\""  PRIx8, value); }

// =====================================================================================================================
void JsonWriter::Value(uint64 value) { FormattedValue("%" PRIu64, value); }
void JsonWriter::Value(uint32 value) { FormattedValue("%" PRIu32, value); }
void JsonWriter::Value(uint16 value) { FormattedValue("%" PRIu16, value); }
void JsonWriter::Value(uint8  value) { FormattedValue("%" PRIu8, value); }

// =====================================================================================================================
void JsonWriter::Value(int64 value) { FormattedValue("%" PRId64, value); }
void JsonWriter::Value(int32 value) { FormattedValue("%" PRId32, value); }
void JsonWriter::Value(int16 value) { FormattedValue("%" PRId16, value); }
void JsonWriter::Value(int8  value) { FormattedValue("%" PRId8, value); }

// =====================================================================================================================
void JsonWriter::Value(float value) { FormattedValue("%g", value); }

// =====================================================================================================================
void JsonWriter::Value(
    bool value)
{
    MaybeNextListEntry();
    TransitionToToken(TokenValue, false);

    const char*const pValue = (value ? "true" : "false");
    const uint32     length = (value ? 4 : 5);

    m_pStream->WriteString(pValue, length);
}

// =====================================================================================================================
void JsonWriter::NullValue()
{
    MaybeNextListEntry();
    TransitionToToken(TokenValue, false);
    m_pStream->WriteString("null", 4);
}

// =====================================================================================================================
// Before a token is written to a list, this must be called to make sure that a comma token is written if necessary.
void JsonWriter::MaybeNextListEntry()
{
    // If we're in a list but not at the beginning, we require a comma token.
    if (TestAnyFlagSet(m_scopeStack[m_curScope], ScopeList) && (m_prevToken != TokenLBracket))
    {
        TransitionToToken(TokenComma, false);
        m_pStream->WriteCharacter(',');
    }
}

// =====================================================================================================================
// Writes any necessary whitespace and updates the previous token. The caller must write out the next token and update
// the scope afterwards.
void JsonWriter::TransitionToToken(
    uint32 nextToken,
    bool   leavingScope)
{
#if PAL_ENABLE_PRINTS_ASSERTS
    PAL_ASSERT(ValidateTransition(nextToken));
#endif

    enum Space : uint8
    {
        SpaceOne  = 1, // One space character.
        SpaceLine = 2  // A newline and space characters needed to reach the proper indentation.
    };

    // Given a transition between any two tokens, this table defines what whitespace (if any) should separate them.
    constexpr uint8 SpaceTable[TokenCount][TokenCount] =
    {
        /* From:     / To: None     LBrace     RBrace     LBracket   RBracket   Comma      Key        Value     */
        /* None     */ { 0,         0,         0,         0,         0,         0,         0,         0         },
        /* LBrace   */ { 0,         0,         0,         SpaceLine, 0,         0,         SpaceLine, 0         },
        /* RBrace   */ { 0,         0,         SpaceLine, 0,         SpaceLine, 0,         0,         0         },
        /* LBracket */ { 0,         SpaceLine, 0,         SpaceLine, 0,         0,         0,         SpaceLine },
        /* RBracket */ { 0,         0,         SpaceLine, 0,         SpaceLine, 0,         0,         0         },
        /* Comma    */ { 0,         SpaceLine, 0,         SpaceLine, 0,         0,         SpaceLine, SpaceLine },
        /* Key      */ { 0,         SpaceOne,  0,         SpaceOne,  0,         0,         0,         SpaceOne  },
        /* Value    */ { 0,         0,         SpaceLine, 0,         SpaceLine, 0,         0,         0         }
    };

    const uint8 spacing = SpaceTable[m_prevToken][nextToken];

    // Note that SpaceLine is forced to SpaceOne if we're in an inline scope.
    if ((spacing == SpaceOne) || ((spacing == SpaceLine) && TestAnyFlagSet(m_scopeStack[m_curScope], ScopeInline)))
    {
        m_pStream->WriteCharacter(' ');
    }
    else if (spacing == SpaceLine)
    {
        // Write IndentSize spaces for each scope after the base ScopeOutside except when we're leaving the current
        // scope in this transition. In that case, we should use one less indent so that the braces/brackets line up.
        const uint32 numSpaces = leavingScope ? ((m_curScope - 1) * IndentSize) : (m_curScope * IndentSize);

        m_pStream->WriteCharacter('\n');
        m_pStream->WriteString(m_indentBuffer, numSpaces);
    }

    // Update the previous token, assuming the caller is going to write it next.
    m_prevToken = nextToken;
}

#if PAL_ENABLE_PRINTS_ASSERTS
// =====================================================================================================================
// Returns true if the previous token and given next token form a valid transition in the current scope. This should
// always return true unless the caller is doing something that breaks the JSON spec (e.g., putting a key in a list).
bool JsonWriter::ValidateTransition(
    uint32 nextToken)
{
    constexpr uint8 ScopeCollection = ScopeList | ScopeMap;

    // Given a transition between any two tokens, this table defines which scopes (if any) permit that transition.
    constexpr uint8 ValidScopes[TokenCount][TokenCount] =
    {
        /* From:     / To:  LBrace        RBrace    LBracket      RBracket   Comma            Key       Value        */
        /* None     */ { 0, ScopeOutside, 0,        ScopeOutside, 0,         0,               0,        ScopeOutside },
        /* LBrace   */ { 0, 0,            ScopeMap, ScopeMap,     0,         0,               ScopeMap, 0            },
        /* RBrace   */ { 0, 0,            ScopeMap, 0,            ScopeList, ScopeCollection, 0,        0            },
        /* LBracket */ { 0, ScopeList,    0,        ScopeList,    ScopeList, 0,               0,        ScopeList    },
        /* RBracket */ { 0, 0,            ScopeMap, 0,            ScopeList, ScopeCollection, 0,        0            },
        /* Comma    */ { 0, ScopeList,    0,        ScopeList,    0,         0,               ScopeMap, ScopeList    },
        /* Key      */ { 0, ScopeMap,     0,        ScopeMap,     0,         0,               0,        ScopeMap     },
        /* Value    */ { 0, 0,            ScopeMap, 0,            ScopeList, ScopeCollection, 0,        0            }
    };

    return TestAnyFlagSet(m_scopeStack[m_curScope], ValidScopes[m_prevToken][nextToken]);
}
#endif

} // Util
