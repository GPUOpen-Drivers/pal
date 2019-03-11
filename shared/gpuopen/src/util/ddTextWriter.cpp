/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "util/ddTextWriter.h"

#include <stdarg.h>
#include <errno.h>

using namespace DevDriver;

//=====================================================================================================================
TextWriter::TextWriter(void* pUserData, TextWriter::WriteBytesCb callback)
    : m_pUserData(pUserData),
      m_pfnWriter(callback),
      m_lastResult(Result::Success)
{
}

//=====================================================================================================================
Result TextWriter::End()
{
    if (CanWrite())
    {
        uint8 nullByte = 0;
        m_lastResult = m_pfnWriter(m_pUserData, &nullByte, 1);
    }
    if (CanWrite())
    {
        // Special "End of Writer" call
        m_lastResult = m_pfnWriter(m_pUserData, nullptr, 0);
    }
    return m_lastResult;
}

//=====================================================================================================================
void TextWriter::WriteText(const char* pString, uint32 length)
{
    if (CanWrite())
    {
        if ((pString != nullptr) && (length > 0))
        {
            length = static_cast<uint32>(strnlen(pString, length));
            for (uint32 i = 0; i < length; i += 1)
            {
                unsigned char c = pString[i];
                if (!(isprint(c) || isspace(c)))
                {
                    DD_PRINT(LogLevel::Debug, "Attempting to write non-writable character \"%c\" (0x%x)\n", c, c);
                    m_lastResult = Result::UriInvalidChar;
                    break;
                }
            }

            if (m_lastResult == Result::Success)
            {
                m_lastResult = m_pfnWriter(m_pUserData, reinterpret_cast<const uint8*>(pString), length);
            }
        }
        // Passing in nullptr is probably an error on the client's end, and we report it back.
        // Asking us to write zero bytes is a no-op and should be predicated on their end.
        // This also looks a lot like our End-of-Writer call, so we mustn't call m_pfnWriter anyway.
        else if ((pString == nullptr) || (length == 0))
        {
            m_lastResult = Result::UriInvalidParameters;
            DD_PRINT(LogLevel::Error,
                     "Calling TextWriter::WriteText(%p, %u) - Invalid parameters. " // concat the strings, no newline
                     "This wouldn't call wouldn't write anything, so we're marking it as an error.",
                     pString,
                     length);
        }
    }
}

//=====================================================================================================================
void TextWriter::Write(const char* pFormat, ...)
{
    // Most formatted text is going to be small, and we can do it in stack allocated space.
    constexpr uint32 bufferSize = 1024;
    char buffer[bufferSize];

    if (CanWrite())
    {
        va_list formatArgs;
        va_start(formatArgs, pFormat);
        // TODO: Make TextWriter::Write(const char*, ...) use the heap if the formatted string would be too long.
        // We should check how large our string will need to be, so that we can spill onto the heap if we have to.
        // We currently do not because of limitations in our snprintf wrapper.
        Platform::Vsnprintf(buffer, bufferSize, pFormat, formatArgs);
        va_end(formatArgs);

        WriteText(buffer, sizeof(buffer));
    }
}

//=====================================================================================================================
void TextWriter::Write(uint64 value)
{
    Write("%llu", value);
}

//=====================================================================================================================
void TextWriter::Write(uint32 value)
{
    Write("%u", value);
}

//=====================================================================================================================
void TextWriter::Write(uint16 value)
{
    Write("%hu", value);
}

//=====================================================================================================================
void TextWriter::Write(uint8 value)
{
    // Print this as an integer, not a character.
    Write("%u", static_cast<uint32>(value)); }

//=====================================================================================================================
void TextWriter::Write(int64 value)
{
    Write("%lld", value);
}

//=====================================================================================================================
void TextWriter::Write(int32 value)
{
    Write("%d", value);
}

//=====================================================================================================================
void TextWriter::Write(int16 value)
{
    Write("%hd", value);
}

//=====================================================================================================================
void TextWriter::Write(double value)
{
    Write("%g", value);
}

//=====================================================================================================================
void TextWriter::Write(float value)
{
    Write("%g", value);
}

//=====================================================================================================================
void TextWriter::Write(bool value)
{
    if (value)
    {
        Write("true");
    }
    else
    {
        Write("false");
    }
}

//=====================================================================================================================
void TextWriter::Write(char value)
{
    if (CanWrite())
    {
        Write("%c", value);
    }
}
