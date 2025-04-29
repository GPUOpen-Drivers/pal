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
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <unistd.h>
#include <stb_sprintf.h>

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

static pthread_once_t s_platformInfoInitOnce = PTHREAD_ONCE_INIT;
static PlatformInfo s_platformInfo = {};

FILE* OpenCacheInfoFile(uint32_t index, const char* name)
{
    const uint32_t FileNameBufSize = 200;
    char filenameBuf[FileNameBufSize] = {};
    stbsp_snprintf(filenameBuf, FileNameBufSize, "/sys/devices/system/cpu/cpu0/cache/index%u/%s", index, name);
    FILE* pFile = fopen(filenameBuf, "r");
    return pFile;
}

ResultEx ReadCacheInfo(uint32_t index, uint32_t* pOutLevel, bool* pOutIsDataCache, uint32_t* pOutCacheSize)
{
    if ((pOutLevel == nullptr) || (pOutIsDataCache == nullptr) || (pOutCacheSize == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    ResultEx result = DD_RESULT_SUCCESS;
    FILE* pFile = OpenCacheInfoFile(index, "level");
    if (pFile)
    {
        uint32_t level = 0;
        int scanned = fscanf(pFile, "%u", &level);
        if (scanned == 1)
        {
            *pOutLevel = level;
        }
        else
        {
            result = DD_RESULT_PARSING_INVALID_STRING;
        }
        fclose(pFile);
        pFile = nullptr;
    }
    else
    {
        result.SetStdError(errno);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        pFile = OpenCacheInfoFile(index, "type");
        if (pFile)
        {
            char typeBuf[16] = {};
            int scanned = fscanf(pFile, "%s", typeBuf);
            if (scanned == 1)
            {
                if (strcmp(typeBuf, "Instruction") == 0)
                {
                    *pOutIsDataCache = false;
                }
                else
                {
                    // Data cache and unified cache both count as data cache.
                    *pOutIsDataCache = true;
                }
            }
            else
            {
                result = DD_RESULT_PARSING_INVALID_STRING;
            }
            fclose(pFile);
            pFile = nullptr;
        }
        else
        {
            result.SetStdError(errno);
        }
    }

    if (result == DD_RESULT_SUCCESS)
    {
        pFile = OpenCacheInfoFile(index, "size");
        if (pFile)
        {
            uint32_t cacheSize = 0;
            int scanned = fscanf(pFile, "%u", &cacheSize);
            if (scanned == 1)
            {
                *pOutCacheSize = cacheSize * 1024;
            }
            else
            {
                result = DD_RESULT_PARSING_INVALID_STRING;
            }
            fclose(pFile);
            pFile = nullptr;
        }
        else
        {
            result.SetStdError(errno);
        }
    }

    return result;
}

DD_RESULT PlatformInfoInitOnce()
{
    // `+` casts the non-capturing lambda to a C style function pointer.
    auto InitFn = +[]() {
        s_platformInfo.initResult = DD_RESULT_SUCCESS;

        // Get page size.

        s_platformInfo.pageSize = getpagesize();

        // Get cache line size. Just use the CPU at index 0.

        s_platformInfo.cacheLineSize = 64; // default to 64 bytes
        FILE* pCacheLineSizeFile = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
        if (pCacheLineSizeFile)
        {
            int scanned = fscanf(pCacheLineSizeFile, "%d", &s_platformInfo.cacheLineSize);
            if (scanned != 1)
            {
                s_platformInfo.initResult = DD_RESULT_PARSING_INVALID_STRING;
            }
            fclose(pCacheLineSizeFile);
        }
        else
        {
            s_platformInfo.initResult.SetStdError(errno);
        }

        // Get cache sizes. We try to read 3 levels.

        uint32_t cacheLevel  = 0;
        bool     isDataCache = false;
        uint32_t cacheSize   = 0;
        for (uint32_t i = 0; i < 4; ++i)
        {
            ResultEx readResult = ReadCacheInfo(i, &cacheLevel, &isDataCache, &cacheSize);
            if ((readResult == DD_RESULT_SUCCESS) && isDataCache)
            {
                DD_ASSERT((cacheLevel > 0) && (cacheLevel <= 3));
                s_platformInfo.cacheSizes[cacheLevel - 1] = cacheSize;
            }
        }
    };

    pthread_once(&s_platformInfoInitOnce, InitFn);

    return s_platformInfo.initResult;
}

} // anonymous namespace

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
