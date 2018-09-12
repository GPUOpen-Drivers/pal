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
  * @file  ddByteReader.h
  * @brief Class to encapsulate valid reading of data from byte buffers.
  ***********************************************************************************************************************
  */
#pragma once
#include "gpuopen.h"

namespace DevDriver
{
    //---------------------------------------------------------------------
    // An object that reads from a byte range into value types.
    // ByteReader performs bounds checking and automatically sizes the reads for the appropriate data type.
    class ByteReader
    {
    public:
        ByteReader(const void* pBegin, const void* pEnd)
            : m_pCurrentByte((const uint8*)pBegin),
              m_pEnd((const uint8*)pEnd)
        {}

        // Get the current byte position in the range.
        const uint8* Get() const
        {
            return m_pCurrentByte;
        }

        // Get the number of remaining bytes in the range.
        size_t Remaining() const
        {
            DD_ASSERT(m_pEnd >= m_pCurrentByte);
            return m_pEnd - m_pCurrentByte;
        }

        // Copies data from bytes to the value type pointer provided.
        // Fails if there are not enough bytes remaining, or if pValue is NULL.
        template <typename T>
        Result Read(T* pValue)
        {
            static_assert(!Platform::IsPointer<T>::Value, "Don't read pointers from byte arrays.");
            size_t bytesToRead = sizeof(T);
            Result result = Result::Error;
            if (bytesToRead <= Remaining())
            {
                if (pValue != nullptr)
                {
                    memcpy(pValue, m_pCurrentByte, bytesToRead);
                    m_pCurrentByte += bytesToRead;
                    result = Result::Success;
                }
                else
                {
                    result = Result::InvalidParameter;
                }
            }
            return result;
        }

        // Move the reading cursor forward nBytes, as if a struct of size nBytes was read with Read().
        // Fails if there are not enough bytes remaining. The current position remains unchanged on failure.
        Result Skip(uint64 nBytes)
        {
            Result result = Result::Error;
            if (nBytes <= Remaining())
            {
                m_pCurrentByte += nBytes;
                result = Result::Success;
            }
            return result;
        }

    private:
        const uint8* m_pCurrentByte;
        const uint8* m_pEnd;
    };
}
