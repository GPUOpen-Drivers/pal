/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include "gpuopen.h"
#include "util/ddTextWriter.h"

#include <stdarg.h>
#include <errno.h>

using namespace DevDriver;

static Result WriteTextViaVectorCb(void* pUserData, const void* pInBytes, size_t numBytes)
{
    Result result = Result::Success;
    Vector<char>& outString  = *static_cast<Vector<char>*>(pUserData);

    // Special "End of Writer" call
    if ((pInBytes == nullptr) && (numBytes == 0))
    {
        // We can use this to flush a buffer or something exotic.
        // Vector<> has no such requirements, so we do nothing.
    }
    // Regular write - copy the buffer out
    else if (pInBytes != nullptr)
    {
        const char* pSrcString = static_cast<const char*>(pInBytes);
        // TODO: If Resize returned whether the allocation succeeded, we could replace the following
        // for-loop with a memcpy/strcpy.
        outString.Reserve(outString.Size() + numBytes);

        for (uint32 i = 0; i < numBytes; i += 1)
        {
            if (!outString.PushBack(pSrcString[i]))
            {
                result = Result::InsufficientMemory;
                break;
            }
        }
    }
    else
    {
        // We should not have landed here - either the pointer is valid, or it's not.
        // To land here, we got a NULL pointer and a non-zero size!
        // This is a programmer error in TextWriter
        DD_PRINT(LogLevel::Warn, "pInBytes=%p, numbytes=%zu", pInBytes, numBytes);
        DD_ASSERT_ALWAYS();
        result = Result::Error;
    }

    return result;
};

//=====================================================================================================================
TextWriter::TextWriter(Vector<char>* pString)
    : m_pUserData(pString),
      m_pfnWriter(WriteTextViaVectorCb),
      m_lastResult(Result::Success)
{
}

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

    // Overwrite the last result with success to allow for subsequent uses of the writer.
    const Result result = m_lastResult;
    m_lastResult = Result::Success;

    return result;
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
                const unsigned char c = pString[i];
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

    // Ensure this buffer is null terminated (just in case!)
    buffer[0] = '\0';

    if (CanWrite())
    {
        va_list formatArgs;
        va_start(formatArgs, pFormat);
        const int32 formattedLength = Platform::Vsnprintf(buffer, bufferSize, pFormat, formatArgs);
        va_end(formatArgs);

        // Snprintf and variants report the size that they *want* to print, not what they actually did.
        if (formattedLength >= 1024)
        {
            // TODO: Make TextWriter::Write(const char*, ...) use the heap if the formatted string would be too long.
            //       TextWriter needs an allocCb first.
            DD_PRINT(
                LogLevel::Error,
                "formatted Write() required more space than was available. sizeof(buffer)=%zu, formattedLength=%d, pFormat=\"%s\"",
                sizeof(buffer),
                formattedLength,
                pFormat);
        }
        else if (formattedLength < 0)
        {
            DD_PRINT(
                LogLevel::Error,
                "vnsprintf encountered an error: vsnprintf returned %d, pFormat=\"%s\"",
                formattedLength,
                pFormat);
        }

        // For now unconditionally write out the buffer
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
    Write("%f", value);
}

//=====================================================================================================================
void TextWriter::Write(float value)
{
    Write("%f", value);
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
