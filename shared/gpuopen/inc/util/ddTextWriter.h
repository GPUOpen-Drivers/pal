/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
  * @file  ddTextWriter.h
  * @brief Class to encapsulate validating and writing text through a callback.
  ***********************************************************************************************************************
  */
#pragma once
#include "ddUriInterface.h"
#include "ddPlatform.h"

namespace DevDriver
{
    class TextWriter final : public ITextWriter
    {
    public:
        // A Callback to write bytes. The user data pointer is passed through the Writer class's constructor.
        // The callback is expected to handle a special "End-Of-Writer" message when
        // both `pBytes` == `nullptr` and `numBytes` == `0`. This is sent during `End()`, and signifies that
        // all writing through this writer is finished.
        // `pUserData` may be `nullptr`.
        typedef Result (*WriteBytesCb)(void* pUserData, const void* pBytes, size_t numBytes);

        // Constructs a `TextWriter` with a callback and its expected user data pointer.
        // `pUserData` may be `nullptr`, if your callback doesn't use it.
        explicit TextWriter(void* pUserData, WriteBytesCb callback);
        virtual ~TextWriter() = default;

        // Finish all writing and return the last error
        // This NULL-terminates the string/buffer through the callback.
        // If you want to inspect the string your callback has been writing to, make sure that this is called!
        Result End() override;

        // Writes formatted text.
        void Write(const char* pFormat, ...) override;

        // Write a value.
        void Write(uint64 value) override;
        void Write(uint32 value) override;
        void Write(uint16 value) override;
        void Write(uint8  value) override;
        void Write(int64  value) override;
        void Write(int32  value) override;
        void Write(int16  value) override;
        void Write(double value) override;
        void Write(float  value) override;
        void Write(bool   value) override;
        void Write(char   value) override;

    private:
        // Write not more than length characters. This is used internally as a funnel for all other write commands.
        // Our text verification is located in here to. (e.g. we error on non-printable characters)
        void WriteText(const char* pString, uint32 length);

        // If the writer's callback returns an error, it saves that error and predicates all of its write functions.
        // This is called before every invocation of the callback.
        bool CanWrite() const
        {
            return (m_lastResult == Result::Success);
        }

        // User-supplied pointer that is passed to every 'm_pfnWriter' call.
        void* const m_pUserData;

        // The callback to write bytes. This is never changed after initialization.
        WriteBytesCb const m_pfnWriter;

        // The last result generated.
        Result m_lastResult;
    };
} // DevDriver
