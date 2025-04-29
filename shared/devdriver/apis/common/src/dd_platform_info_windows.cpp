/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_platform_info.h>
#include <dd_assert.h>
#include <dd_result.h>
#include <cstdlib>
#include <Windows.h>

namespace
{

using namespace DevDriver;

struct PlatformInfo
{
    ResultEx initResult;
    uint32_t pageSize;
    uint32_t cacheLineSize;
    uint32_t cacheSizes[3];
};

static INIT_ONCE s_platformInitOnce = INIT_ONCE_STATIC_INIT;
static PlatformInfo s_platformInfo = {};

BOOL WINAPI InitOnceCallback(PINIT_ONCE, PVOID pUserdata, PVOID*)
{
        auto pPlatformInfo = reinterpret_cast<PlatformInfo*>(pUserdata);

        pPlatformInfo->initResult = DD_RESULT_SUCCESS;
        BOOL success = FALSE;

        // Get page size.

        SYSTEM_INFO sysInfo {};
        GetSystemInfo(&sysInfo);
        pPlatformInfo->pageSize = sysInfo.dwPageSize;

        // Get cache sizes and cache line size.

        pPlatformInfo->cacheLineSize = 64; // Default to 64 bytes.
        DWORD cacheInfoBufSize = 0;
        success = GetLogicalProcessorInformationEx(RelationCache, NULL, &cacheInfoBufSize);
        if ((success == FALSE) && (GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            auto pCacheInfoList = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)std::malloc(cacheInfoBufSize);
            success = GetLogicalProcessorInformationEx(RelationCache, pCacheInfoList, &cacheInfoBufSize);
            if (success)
            {
                DD_ASSERT(pCacheInfoList[0].Relationship == RelationCache);
                pPlatformInfo->cacheLineSize = pCacheInfoList[0].Cache.LineSize;

                DD_ASSERT((pCacheInfoList[0].Cache.Level > 0) && (pCacheInfoList[0].Cache.Level <= 3));
                pPlatformInfo->cacheSizes[pCacheInfoList[0].Cache.Level - 1] = pCacheInfoList[0].Cache.CacheSize;

                auto pCacheInfoListEnd = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(
                    (uint8_t*)pCacheInfoList + cacheInfoBufSize);

                // Check 3 more cache info entries.
                auto pNextCacheInfoEntry = pCacheInfoList;
                for (uint32_t i = 0; i < 3; ++i)
                {
                    pNextCacheInfoEntry = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*)(
                        (uint8_t*)pNextCacheInfoEntry + pNextCacheInfoEntry->Size);
                    if (pNextCacheInfoEntry < pCacheInfoListEnd)
                    {
                        if ((pNextCacheInfoEntry->Cache.Type == PROCESSOR_CACHE_TYPE::CacheData) ||
                            (pNextCacheInfoEntry->Cache.Type == PROCESSOR_CACHE_TYPE::CacheUnified))
                        {
                            DD_ASSERT((pNextCacheInfoEntry->Cache.Level > 0) && (pNextCacheInfoEntry->Cache.Level <= 3));
                            pPlatformInfo->cacheSizes[pNextCacheInfoEntry->Cache.Level - 1] =
                                pNextCacheInfoEntry->Cache.CacheSize;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                pPlatformInfo->initResult.SetWin32Error(GetLastError());
            }
        }
        else
        {
            pPlatformInfo->initResult.SetWin32Error(GetLastError());
        }

        return success;
}

ResultEx PlatformInfoInitOnce()
{
    InitOnceExecuteOnce(&s_platformInitOnce, InitOnceCallback, &s_platformInfo, NULL);
    return s_platformInfo.initResult;
}

}; // anonymous namespace

namespace DevDriver
{

ResultEx PlatformInfo::Init()
{
    return PlatformInfoInitOnce();
}

uint32_t PlatformInfo::GetPageSize()
{
    PlatformInfoInitOnce();
    return s_platformInfo.pageSize;
}

uint32_t PlatformInfo::GetCacheLineSize()
{
    PlatformInfoInitOnce();
    return s_platformInfo.cacheLineSize;
}

uint32_t PlatformInfo::GetL1CacheSize()
{
    PlatformInfoInitOnce();
    return s_platformInfo.cacheSizes[0];
}

uint32_t PlatformInfo::GetL2CacheSize()
{
    PlatformInfoInitOnce();
    return s_platformInfo.cacheSizes[1];
}

uint32_t PlatformInfo::GetL3CacheSize()
{
    PlatformInfoInitOnce();
    return s_platformInfo.cacheSizes[2];
}

} // namespace DevDriver
