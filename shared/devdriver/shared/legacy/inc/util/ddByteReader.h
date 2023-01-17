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

#include <ddPlatform.h>

namespace DevDriver
{
    //---------------------------------------------------------------------
    // An object that reads from a byte range into value types.
    // ByteReader performs bounds checking and automatically sizes the reads for the appropriate data type.
    class ByteReader
    {
    public:
        ByteReader(const void* pData, size_t dataSize)
            : m_pCur(pData),
              m_pEnd(VoidPtrInc(pData, dataSize))
        {}

        // Get the number of remaining bytes in the range.
        size_t Remaining() const
        {
            const uint8* pCur = reinterpret_cast<const uint8*>(m_pCur);
            const uint8* pEnd = reinterpret_cast<const uint8*>(m_pEnd);

            DD_ASSERT(pEnd >= pCur);
            return (pEnd - pCur);
        }

        // Returns true if there are more bytes to read
        bool HasBytes() const { return (Remaining() > 0); }

        // Returns a pointer to a sized subset of the byte array based on the current position
        // Fails if there are not enough bytes remaining, or if ppValue is NULL.
        Result GetBytes(const void** ppData, size_t dataSize)
        {
            Result result = Result::InvalidParameter;

            if ((ppData != nullptr) && (dataSize > 0))
            {
                if (dataSize <= Remaining())
                {
                    *ppData = m_pCur;
                    m_pCur = VoidPtrInc(m_pCur, dataSize);

                    result = Result::Success;
                }
                else
                {
                    result = Result::Error;
                }
            }

            return result;
        }

        // Returns a type pointer at the current byte array position
        // Fails if there are not enough bytes remaining, or if ppValue is NULL.
        template <typename T>
        Result Get(const T** ppValue)
        {
            static_assert(!Platform::IsPointer<T>::Value, "Don't read pointers from byte arrays.");

            return GetBytes(reinterpret_cast<const void**>(ppValue), sizeof(T));
        }

        // Copies data from bytes to the buffer pointer provided.
        // Fails if there are not enough bytes remaining, or if pValue is NULL.
        Result ReadBytes(void* pDst, size_t numBytes)
        {
            Result result = Result::InvalidParameter;

            if ((pDst != nullptr) && (numBytes > 0))
            {
                const void* pBytes = nullptr;
                result = GetBytes(&pBytes, numBytes);
                if (result == Result::Success)
                {
                    memcpy(pDst, pBytes, numBytes);
                }
            }

            return result;
        }

        // Copies data from bytes to the value type pointer provided.
        // Fails if there are not enough bytes remaining, or if pValue is NULL.
        template <typename T>
        Result Read(T* pValue)
        {
            static_assert(!Platform::IsPointer<T>::Value, "Don't read pointers from byte arrays.");

            Result result = Result::InvalidParameter;

            if (pValue != nullptr)
            {
                result = ReadBytes(reinterpret_cast<void*>(pValue), sizeof(T));
            }

            return result;
        }

        // Move the reading cursor forward numBytes, as if a struct of size numBytes was read with Read().
        // Fails if there are not enough bytes remaining. The current position remains unchanged on failure.
        Result Skip(size_t numBytes)
        {
            Result result = Result::Error;
            if (numBytes <= Remaining())
            {
                m_pCur = VoidPtrInc(m_pCur, numBytes);
                result = Result::Success;
            }
            return result;
        }

    private:
        const void* m_pCur;       // Current byte position
                                  // This is incremented as data is read/get
        const void* const m_pEnd; // End byte position
                                  // This is set during the constructor and never changed. It's used to calculate the
                                  // number of bytes remaining.
    };

    // Get infers a size from the type and void* has no size.
    // Use GetBytes() with an explicit size instead.
    template <>
    Result ByteReader::Get(const void** ppValue) = delete;

    // Read infers a size from the type and void* has no size.
    // Use ReadBytes() with an explicit size instead.
    template <>
    Result ByteReader::Read(void* pValue) = delete;
}
