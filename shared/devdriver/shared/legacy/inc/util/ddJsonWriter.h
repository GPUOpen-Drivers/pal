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
#pragma once
#include "ddUriInterface.h"
#include "ddPlatform.h"

#include "ddTextWriter.h"

// RapidJson. We use the SAX API:
//      http://rapidjson.org/md_doc_sax.html#Writer

// Turn on all the bells and whistles
// TODO: We may want to try using SSE2 for improved string parsing performance.
#define RAPIDJSON_WRITE_DEFAULT_FLAGS (kWriteValidateEncodingFlag | kWriteNanAndInfFlag)

#define RAPIDJSON_ASSERT(x) DD_ASSERT(x)
#include "rapidjson/writer.h"

namespace DevDriver
{
    class JsonWriter final : public IStructuredWriter
    {
    public:
        virtual ~JsonWriter() = default;

        // A Callback to write bytes. The user data pointer is passed through the Writer class's constructor.
        // The callback is expected to handle a special "End-Of-Writer" message when
        // both `pBytes` == `nullptr` and `numBytes` == `0`. This is sent during `End()`, and signifies that
        // all writing through this writer is finished.
        // `pUserData` may be `nullptr`.
        // All of the Json writes go through this callback via a TextWriter.
        using  WriteBytesCb = TextWriter::WriteBytesCb;

        // Write text into a Vector<char>
        explicit JsonWriter(Vector<char>* pString)
            : m_textStream(pString),
              m_rjWriter(m_textStream),
              m_lastResult(Result::Success)
        {}

        // Constructs a `JsonWriter` with a callback and its expected user data pointer.
        // `pUserData` may be `nullptr`, if your callback doesn't use it.
        explicit JsonWriter(void* pUserData, JsonWriter::WriteBytesCb callback)
            : m_textStream(pUserData, callback),
              m_rjWriter(m_textStream),
              m_lastResult(Result::Success)
        {}

        // Finish all writing and return the last error.
        Result End() override;

        // ===== Collection Writers ====================================================================================

        // Begin writing a new list collection.
        void BeginList() override;

        // End the current list collection.
        void EndList() override;

        // Begin writing a new map collection.
        void BeginMap() override;

        // End the current map collection.
        void EndMap() override;

        // Write a key into a map.
        void Key(const char* pKey) override;

        // ===== Value Writers =========================================================================================

        // Write a string value.
        void Value(const char* pValue) override;
        void Value(const char* pValue, size_t length) override;

        // Write a value.
        void Value(uint64 value) override;
        void Value(uint32 value) override;
        void Value(uint16 value) override;
        void Value(uint8  value) override;
        void Value(int64  value) override;
        void Value(int32  value) override;
        void Value(int16  value) override;
        void Value(int8   value) override;
        void Value(double value) override;
        void Value(float  value) override;
        void Value(char   value) override;
        void Value(bool   value) override;

        // Write a JSON "null" value.
        void ValueNull() override;

    private:
        // Implements rapidjson's Stream interface, for use by rapidjson::Writer.
        class JsonTextStream
        {
        private:
            TextWriter textWriter;

        public:
            JsonTextStream(Vector<char>* pString)
                : textWriter(pString)
            {}
            JsonTextStream(void* pUserData, TextWriter::WriteBytesCb callback)
                : textWriter(pUserData, callback)
            {}
            ~JsonTextStream() = default;

            Result End()
            {
                return textWriter.End();
            }

            // ----- RapidJSON's Stream interface ---------------------------------------------------------------------
            using Ch = char;

            void Put(Ch c) { textWriter.Write(c); }
            void Flush()   { /* Nothing to do. */ }
        };

        // If the writer's callback returns an error, it saves that error and predicates all of its write functions.
        // This is called before every invocation of the callback.
        bool CanWrite() const
        {
            return (m_lastResult == Result::Success);
        }

        // Wraps a TextWriter in a rapidjson-compatible interface.
        JsonTextStream m_textStream;

        // Writes a densely-formatted Json stream. e.g.: `{"my_key":1.25,"key":["list","array"]}`
        // It performs its own sanity checks on the Json.
        rapidjson::Writer<JsonTextStream> m_rjWriter;

        // The last result generated.
        // This should be specific to the Json part of things.
        // If there's an issue with the TextWriter, then `m_textStream` will
        // keep track of it.
        Result m_lastResult;
    };

} // DevDriver
