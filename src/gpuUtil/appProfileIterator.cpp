/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2017-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAppProfileIterator.h"
#include "palInlineFuncs.h"

using namespace Util;

namespace GpuUtil
{

// =====================================================================================================================
// Build the Content Distribution Network (CDN) application ID string.
bool QueryAppContentDistributionId(
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
    wchar_t*       pContentDistributionId,
#else
    char*          pContentDistributionId,
#endif
    size_t         bufferLength)
{
    bool match = false;
    if ((pContentDistributionId != nullptr) && (bufferLength > 1))
    {
        // If environment variable exists, for example SteamAppId is 570, build the string "SteamAppId:570"
        constexpr uint32 NumOfEnvVars = 4;

        // This strings are used for Steam, Ubisoft's UPlay, and EA's Origin..
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
        const wchar_t* pEnvVarName[NumOfEnvVars] = { L"SteamAppId", L"upc_product_id", L"ContentId", L"EALaunchCode" };
#else
        const char* pEnvVarName[NumOfEnvVars] = { "SteamAppId", "upc_product_id", "ContentId", "EALaunchCode" };
#endif

        for (uint32 id = 0; id < NumOfEnvVars; id++)
        {
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
            wchar_t* pEnvVarValue;
            pEnvVarValue = _wgetenv(pEnvVarName[id]);
#else
            char* pEnvVarValue;
            pEnvVarValue = getenv(pEnvVarName[id]);
#endif

            if (pEnvVarValue != nullptr)
            {
                match = true;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 759
                Util::Wcsncpy(pContentDistributionId, pEnvVarName[id], bufferLength);
                Util::Wcscat(pContentDistributionId, L":", bufferLength);
                Util::Wcscat(pContentDistributionId, pEnvVarValue, bufferLength);
#else
                Util::Strncpy(pContentDistributionId, pEnvVarName[id], bufferLength);
                Util::Strncat(pContentDistributionId, bufferLength, ":");
                Util::Strncat(pContentDistributionId, bufferLength, pEnvVarValue);
#endif
                break;
            }
        }
    }

    return match;
}

// =====================================================================================================================
AppProfileIterator::AppProfileIterator(
    const void* pData)
{
}

// =====================================================================================================================
AppProfileIterator::~AppProfileIterator()
{
}

// =====================================================================================================================
// Returns true unless the iterator has advanced past the end of profile packet
bool AppProfileIterator::IsValid() const
{
    return false;
}

// =====================================================================================================================
// Move iterator to next value
void AppProfileIterator::Next()
{
}

// =====================================================================================================================
// Reset the iterator to the beginning of value packet
void AppProfileIterator::Restart()
{
}

// =====================================================================================================================
// Returns the pointer to the name of current property.
const char* AppProfileIterator::GetName() const
{
    return nullptr;
}

// =====================================================================================================================
// Returns the size of data of current property.
uint32 AppProfileIterator::GetDataSize() const
{
    return 0;
}

// =====================================================================================================================
// Returns the pointer to the data of current property.
const void* AppProfileIterator::GetData() const
{
    return nullptr;
}

} // GpuUtil

