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
* @file  string.h
* @brief Templated string class for gpuopen
***********************************************************************************************************************
*/

#pragma once

#include <string.h>

namespace DevDriver
{
    // A String class that stores the string inline with a compile-time maximum size.
    // This class facilitiates passing bounded sized C Strings around without dynamic allocation. It has POD semantics
    // when copied or passed by value into functions, and can be stored in a vector.
    template <size_t FixedSize>
    class FixedString
    {
    private:
        char m_data[FixedSize];

    public:
        FixedString()
        {
            // Only the first byte needs to be initialized - we actively do not want to zero the entire array!
            m_data[0] = 0;
        }

        FixedString(const FixedString<FixedSize>&) = default;
        FixedString(FixedString<FixedSize>&&) = default;

        ~FixedString() {}

        FixedString<FixedSize>& operator=(FixedString<FixedSize>& pOther)
        {
            Platform::Strncpy(m_data, pOther.m_data, sizeof(m_data));
            return *this;
        }

        FixedString<FixedSize>& operator=(FixedString<FixedSize>&& pOther)
        {
            Platform::Strncpy(m_data, pOther.m_data, sizeof(m_data));
            return *this;
        }

        bool operator==(const FixedString<FixedSize>& other) const
        {
            return strncmp(this->AsCStr(), other.AsCStr(), FixedSize) == 0;
        }

        bool operator!=(const FixedString<FixedSize>& other) const
        {
            return strncmp(this->AsCStr(), other.AsCStr(), FixedSize) != 0;
        }

        // Create a FixedString from a C String, truncating the copy if pString is too long
        explicit FixedString(const char* pString)
        {
            Platform::Strncpy(m_data, pString, sizeof(m_data));
        }

        // Return a pointer to the inline C String.
        const char* AsCStr() const
        {
            return m_data;
        }

        char* AsCStr()
        {
            return m_data;
        }

        // Computes the length of the string.
        // Note! This is an O(N) operation!
        size_t Size() const
        {
            return strlen(m_data);
        }
    };

    // Sanity check for class size.
    static_assert(sizeof(FixedString<16>) == 16, "FixedString<16> should be exactly 16 bytes");

} // DevDriver
