/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"

namespace GpuUtil
{

/// Build the Content Distribution Network (CDN) application ID string.
///
/// This function checks the environment variables for one of a few key values used by content distribution networks
/// like Steam, EA Origin, and Ubisift Uplay.  If found, the data is formed into a unique string, like
/// "SteamAppId:570".  This approach is used Radeon Settings to identify installed apps and store profiles.
/// This string can be used to query application profiles in place of the EXE name via
/// IPlatform::QueryRawApplicationProfile().
///
/// @param [in/out] pContentDistributionId  Pre-allocated buffer passed in by the caller, filled by function.
/// @param [in]     bufferlength            Length of pre-allocated buffer passed in by the caller.
///
/// @returns true if a match is found, otherwise false.
extern bool QueryAppContentDistributionId(
    wchar_t* pContentDistributionId,
    size_t   bufferLength);

/**
 ***********************************************************************************************************************
 * @brief Iterator for traversal of properties in profile packet.
 *        @see IPlatform::QueryRawApplicationProfile for more information
 ***********************************************************************************************************************
 */
class AppProfileIterator
{
public:
    /// Constructor
    AppProfileIterator(const void* pData);

    /// Destructor
    ~AppProfileIterator();

    /// Returns true unless the iterator has advanced past the end of profile packet
    bool IsValid() const;

    /// Returns the pointer to the name of current property.
    const char* GetName() const;

    /// Returns the size of data of current property.
    Util::uint32 GetDataSize()  const;

    /// Returns the pointer to the data of current property.
    const void* GetData() const;

    /// Advances the iterator to the next property.
    void Next();

    /// Moves back the iterator to the beginning of profile packet.
    void Restart();

private:
    PAL_DISALLOW_DEFAULT_CTOR(AppProfileIterator);
    PAL_DISALLOW_COPY_AND_ASSIGN(AppProfileIterator);
};

} // GpuUtil
