/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palAssert.h"
#include "palSysUtil.h"
#include "palInlineFuncs.h"
#include "palFile.h"
#include "palSysMemory.h"
#include "palMemTrackerImpl.h"
#include "palHashMapImpl.h"

#include <cwchar>
#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/input.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <poll.h>

namespace Util
{

// =====================================================================================================================
// Get affinity mask of each core complex for AMD processor
static Result GetCcxMask(
    SystemInfo* pSystemInfo,
    uint32 totalLogicalCoreCount)
{
    uint32  idx      = 0;
    uint32  maxCount = 0;
    uint32* pData    = nullptr;

    switch (pSystemInfo->cpuType)
    {
        case CpuType::AmdRyzen:
            maxCount = RyzenMaxCcxCount;
            pData    = pSystemInfo->cpuArchInfo.amdRyzen.affinityMask;
            break;
        default:
            // Not implemented for other cpu except Ryzen
            PAL_NOT_IMPLEMENTED();
            break;
    }

    // According to spec, CPUID with EXA=0x8000001D, ECX=0x03
    // CPUID_Fn8000001D_EAX_x03 [Cache Properties (L3)] (CachePropEax3)
    // Bits(25:14)  NumSharingCache: number of logical processors sharing this cache.
    //              The number of logical processors sharing this cache is NumSharingCache+1.
    uint32 reg[4] = {0, 0, 0, 0};
    CpuId(reg, 0x8000001D, 0x3);

    uint32 logicalProcessorMask = (1 << (reg[0] + 1)) - 1;
    maxCount = Min(maxCount, totalLogicalCoreCount / (reg[0] + 1));

    // each CCX has a mask like these: 0xff, 0xff00, ...
    while (idx < maxCount)
    {
        pData[idx] = logicalProcessorMask;
        idx++;
        logicalProcessorMask = logicalProcessorMask << (reg[0] + 1);
    }

    return Result::Success;
}

// =====================================================================================================================
// Queries system information.
Result QuerySystemInfo(
    SystemInfo* pSystemInfo)
{
    Result result = Result::ErrorInvalidPointer;

    if (pSystemInfo != nullptr)
    {
        uint32 regValues[4] = {};

        // Query the vendor string
        CpuId(regValues, 0);

        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[0]) = regValues[1]; // EBX
        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[4]) = regValues[3]; // EDX
        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[8]) = regValues[2]; // ECX

        // Null-terminate the string. The returned vendor string is always 12 bytes and does not include a terminator.
        pSystemInfo->cpuVendorString[12] = '\0';

        // CPUID instruction constants
        constexpr uint32 BrandStringFunctionId      = 0x80000000;
        constexpr uint32 BrandStringFunctionIdBegin = 0x80000002;
        constexpr uint32 BrandStringFunctionIdEnd   = 0x80000004;

        // Query the max supported brand string function
        CpuId(regValues, BrandStringFunctionId);

        if (regValues[0] >= BrandStringFunctionIdEnd)
        {
            for (uint32 funcIndex = 0; funcIndex < 3; ++funcIndex)
            {
                const uint32 offset = (funcIndex * (sizeof(uint32) * 4));
                char* pBrandStringBase = (pSystemInfo->cpuBrandString + offset);
                CpuId(reinterpret_cast<uint32*>(pBrandStringBase),
                        BrandStringFunctionIdBegin + funcIndex);
            }

            // No need to null-terminate here since the returned string is always terminated.

            result = Result::Success;
        }
        else
        {
            result = Result::ErrorUnavailable;
        }

        if (result == Result::Success)
        {
            if (strcmp(pSystemInfo->cpuVendorString, "AuthenticAMD") == 0)
            {
                QueryAMDCpuType(pSystemInfo);
            }
            else if (strcmp(pSystemInfo->cpuVendorString, "GenuineIntel") == 0)
            {
                QueryIntelCpuType(pSystemInfo);
            }
            else
            {
                pSystemInfo->cpuType = CpuType::Unknown;
            }
        }

        pSystemInfo->cpuLogicalCoreCount  = 0;
        pSystemInfo->cpuPhysicalCoreCount = 0;

        // parse /proc/cpuinfo to get logical and physical core info
        File cpuInfoFile;
        cpuInfoFile.Open("/proc/cpuinfo", FileAccessRead);

        if ((result == Result::Success) && cpuInfoFile.IsOpen())
        {
            struct CpuCoreCount
            {
                uint32 logicalCoreCount;
                uint32 physicalCoreCount;
            };
            typedef Util::HashMap<uint32, CpuCoreCount, GenericAllocatorAuto> PhysicalPackageCoreCountMap;

            constexpr size_t     BufSize            = 8*1024;
            constexpr uint32     MaxSocketsHint     = 4;
            CpuCoreCount*        pCoreCount         = nullptr;
            bool                 coreCountPopulated = false;
            GenericAllocatorAuto allocator;
            uint32               cpuClockSpeedTotal = 0;

            auto pBuf = static_cast<char* const>(PAL_CALLOC(BufSize, &allocator, AllocInternalTemp));
            PhysicalPackageCoreCountMap coreCountPerPhysicalId(MaxSocketsHint, &allocator);

            result = coreCountPerPhysicalId.Init();

            while ((pBuf != nullptr) && (result == Result::Success))
            {
                result = cpuInfoFile.ReadLine(pBuf, BufSize, nullptr);

                if (result == Result::Success)
                {
                    auto pMatchStr = strstr(pBuf, "physical id");
                    if (pMatchStr != nullptr)
                    {
                        uint32 physicalId = 0;

                        if (sscanf(pMatchStr, "physical id : %d", &physicalId) == 1)
                        {
                            result = coreCountPerPhysicalId.FindAllocate(physicalId,
                                                                         &coreCountPopulated,
                                                                         &pCoreCount);
                        }
                        else
                        {
                            PAL_ASSERT_ALWAYS();
                        }
                        continue;
                    }

                    pMatchStr = strstr(pBuf, "cpu MHz");
                    if (pMatchStr != nullptr)
                    {
                        uint32 cpuClockSpeed = 0;

                        if (sscanf(pMatchStr, "cpu MHz : %d", &cpuClockSpeed) == 1)
                        {
                            cpuClockSpeedTotal += cpuClockSpeed;
                        }
                        else
                        {
                            PAL_ASSERT_ALWAYS();
                        }
                        continue;
                    }

                    if ((coreCountPopulated == false) && (pCoreCount != nullptr))
                    {
                        pMatchStr = strstr(pBuf, "siblings");
                        if (pMatchStr != nullptr)
                        {
                            auto ret = sscanf(pMatchStr, "siblings : %d", &pCoreCount->logicalCoreCount);
                            PAL_ASSERT(ret == 1);
                            continue;
                        }

                        pMatchStr = strstr(pBuf, "cpu cores");
                        if (pMatchStr != nullptr)
                        {
                            auto ret = sscanf(pMatchStr, "cpu cores : %d", &pCoreCount->physicalCoreCount);
                            PAL_ASSERT(ret == 1);
                            continue;
                        }
                    }
                }
                else // EOF or an error
                {
                    if (result == Result::Eof)
                    {
                        result = Result::Success;
                    }
                    break;
                }
            }

            PAL_FREE(pBuf, &allocator);

            if (result == Result::Success)
            {
                for (auto iter = coreCountPerPhysicalId.Begin(); iter.Get() != nullptr; iter.Next())
                {
                    const auto& coreCount = iter.Get()->value;

                    pSystemInfo->cpuLogicalCoreCount  += coreCount.logicalCoreCount;
                    pSystemInfo->cpuPhysicalCoreCount += coreCount.physicalCoreCount;
                }
                pSystemInfo->cpuFrequency = cpuClockSpeedTotal / pSystemInfo->cpuLogicalCoreCount;
            }
        }

        cpuInfoFile.Close();

        // GetCcxMask() should be called only for Ryzen for now.
        if ((result == Result::Success) && (pSystemInfo->cpuType == CpuType::AmdRyzen))
        {
            result = GetCcxMask(pSystemInfo, pSystemInfo->cpuLogicalCoreCount);
        }

        if (result == Result::Success)
        {
             uint64 totalMemByteSize = sysconf( _SC_PHYS_PAGES ) * sysconf( _SC_PAGESIZE );
             pSystemInfo->totalSysMemSize = static_cast<uint32>(totalMemByteSize / 1024 / 1024);
        }
    }

    return result;
}

// =====================================================================================================================
// Retrieves the frequency of the performance counter for CPU times.
// in current implementation, the tick has been set to 1ns.
int64 GetPerfFrequency()
{
    constexpr uint32 NanosecsPerSec = (1000 * 1000 * 1000);

    return NanosecsPerSec;
}

// =====================================================================================================================
// Retrieves the current value of the performance counter, which is a high resolution time stamp that can be used for
// time-interval measurements.
int64 GetPerfCpuTime()
{
    int64 time = 0LL;

    // clock_gettime() returns the monotonic time since EPOCH
    timespec ts = { };
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0)
    {
        // the tick takes 1 ns since we actually can hardly get timer with res less than 1ns.
        constexpr int64 NanosecsPerSec = 1000000000LL;

        time = ((ts.tv_sec * NanosecsPerSec) + ts.tv_nsec);
    }

    return time;
}

// =====================================================================================================================
bool KeyTranslate(
    int      input,
    KeyCode* pCode)
{
    bool ret = false;
    PAL_ASSERT(pCode != nullptr);

    switch (input)
    {
    case KEY_F10:
        *pCode = KeyCode::F10;
        ret    = true;
        break;
    case KEY_F11:
        *pCode = KeyCode::F11;
        ret    = true;
        break;
    case KEY_F12:
        *pCode = KeyCode::F12;
        ret    = true;
        break;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        *pCode = KeyCode::Shift;
        ret    = true;
        break;
    default:
        break;
    }

    return ret;
}

// =====================================================================================================================
static bool FindKeyboardDeviceNode(
    char *pNodeName)
{
    struct dirent** ppNameList = nullptr;
    char            path[128]  = "/dev/input/by-path/";
    bool            ret        = false;

    // iterate the files on the directory
    const int32 numDirs = scandir(path, &ppNameList, 0, alphasort);

    for (int32 dirIdx = 0; dirIdx < numDirs; dirIdx++)
    {
        // 'kbd' is the key word that refers to the keyboard.
        // Note: it is still required to iterate all ppNameList to free memory in case the node has been found
        if ((ret == false) && (strstr(ppNameList[dirIdx]->d_name, "kbd")))
        {
            strcpy(pNodeName, path);
            // construct the absolute path for the soft link
            strcat(path, ppNameList[dirIdx]->d_name);
            char linkName[64] = {0};
            // get the relative path for the keyboard's device node
            int32 n = readlink(path, linkName, sizeof(linkName));
            if (n > 0)
            {
                strncat(pNodeName, linkName, n);
                ret = true;
            }
        }
        free(ppNameList[dirIdx]);
    }

    if (ppNameList)
    {
        free(ppNameList);
    }

    return ret;
}

// =====================================================================================================================
// Reports whether the specified key has been pressed down.
bool IsKeyPressed(
    KeyCode key,
    bool*   pPrevState)
{
    char              devName[128]  = {0};
    static const bool ret           = FindKeyboardDeviceNode(devName);
    static int        device        = ret ? open(devName, O_RDONLY|O_NONBLOCK) : -1;

    bool isKeySet = false;

    struct input_event ev = {};

    // return false if we cannot get the device node.
    int retVal = (device == -1) ? -1 : 0;

    KeyCode keys[2] = {};

    int maxIndex = IsComboKey(key, keys) ? 1 : 0;

    int index = 0;

    // 4 is a heuristic number
    int retry = 4;

    while ((retVal >= 0) && (index <= maxIndex))
    {
        retVal = read(device,&ev, sizeof(ev));

        if ((retVal   >= 0) &&  // The read do grab some event back.
            (ev.type  == 1) &&  // The event is EV_KEY
            (ev.value == 1))    // 0: key release 1: key pressed 2: key auto repeat
        {
            KeyCode keyGet;
            if (KeyTranslate(ev.code, &keyGet))
            {
                if (keyGet == keys[index])
                {
                    if (index == maxIndex)
                    {
                        isKeySet = true;
                        break;
                    }
                    else
                    {
                        index ++;
                    }
                }
            }
        }
        else if (retVal == -1)
        {
            // if errno is not EAGAIN, we should just close the device.
            if (errno != EAGAIN)
            {
                close(device);
                device = -1;
            }
            else if ((index > 0) && (retry > 0))
            {
                // poll at most 100ms in case it is a second key of a combo key.
                int ret = -1;
                do
                {
                    struct pollfd fd = {};
                    fd.fd            = device;
                    fd.events        = POLLIN;

                    ret = poll(&fd, 1, 100);

                    // retry if there are something to read
                    if ((ret > 0) && (fd.revents == POLLIN))
                    {
                        // try to read once more
                        retVal = 0;
                        retry --;
                    }
                    // ret == 0 means the timout happens
                    // therefore, we don't need to retry the read but kick off next round of polling.
                    else if (ret == 0)
                    {
                        retVal = 0;
                        retry --;
                    }
                } while (ret == 0);
            }
        }
    }

    // On windows, the pPrevState is supposed to provide an aux-state so that IsKeyPressed can identify
    // the *pressed* event, but it is not needed for Linux.
    // Just set it to isKeySet to maintain the correct prev-state.
    if (pPrevState != nullptr)
    {
        *pPrevState = isKeySet;
    }

    return isKeySet;
}

// =====================================================================================================================
// Retrieves the path and filename of the current executable.
Result GetExecutableName(
    char*  pBuffer,
    char** ppFilename,
    size_t bufferLength)
{
    Result result = Result::Success;

    PAL_ASSERT((pBuffer != nullptr) && (ppFilename != nullptr));

    const ssize_t count = readlink("/proc/self/exe", pBuffer, bufferLength - sizeof(char));
    if (count >= 0)
    {
        // readlink() doesn't append a null terminator, so we must do this ourselves!
        pBuffer[count] = '\0';
    }
    else
    {
        // The buffer was insufficiently large, just return an empty string.
        pBuffer[0] = '\0';
        PAL_ALERT_ALWAYS();

        result = Result::ErrorInvalidMemorySize;
    }

    char*const pLastSlash = strrchr(pBuffer, '/');
    (*ppFilename) = (pLastSlash == nullptr) ? pBuffer : (pLastSlash + 1);

    return result;
}

// =====================================================================================================================
// Retrieves the wchar_t path and filename of the current executable.
Result GetExecutableName(
    wchar_t*  pWcBuffer,
    wchar_t** ppWcFilename,
    size_t    bufferLength)
{
    Result result = Result::Success;

    PAL_ASSERT((pWcBuffer != nullptr) && (ppWcFilename != nullptr));

    char buffer[PATH_MAX];

    const ssize_t count = readlink("/proc/self/exe", &buffer[0], Min(bufferLength, sizeof(buffer)) - sizeof(char));

    // Convert to wide char
    if (count >= 0)
    {
        // According to MSDN, if mbstowcs encounters an invalid multibyte character, it returns -1
        int success = (int)mbstowcs(pWcBuffer, static_cast<char *>(buffer), count);
        PAL_ASSERT(success > 0);

        // readlink() doesn't append a null terminator, so we must do this ourselves!
        pWcBuffer[count] = L'\0';
    }
    else
    {
        // The buffer was insufficiently large, just return an empty string.
        pWcBuffer[0] = L'\0';
        PAL_ALERT_ALWAYS();

        result = Result::ErrorInvalidMemorySize;
    }

    wchar_t*const pLastSlash = wcsrchr(pWcBuffer, L'/');
    (*ppWcFilename) = (pLastSlash == nullptr) ? pWcBuffer : (pLastSlash + 1);

    return result;
}

// =====================================================================================================================
// Splits a filename into its path and file components.
void SplitFilePath(
    const char* pFullPath,
    char*       pPathBuf,
    size_t      pathLen,
    char*       pFileBuf,
    size_t      fileLen)
{
    char tempBuf[PATH_MAX] = { };

    if (pPathBuf != nullptr)
    {
        // NOTE: dirname() may modify the input buffer, so we need to duplicate the caller's input string first!
        Strncpy(&tempBuf[0], pFullPath, sizeof(tempBuf));

        const char*const pDirName = dirname(&tempBuf[0]);
        PAL_ASSERT(pDirName != nullptr);

        Strncpy(pPathBuf, pDirName, pathLen);
    }

    if (pFileBuf != nullptr)
    {
        // NOTE: basename() may modify the input buffer, so we need to duplicate the caller's input string first!
        Strncpy(&tempBuf[0], pFullPath, sizeof(tempBuf));

        const char*const pBaseName = basename(&tempBuf[0]);
        PAL_ASSERT(pBaseName != nullptr);

        Strncpy(pFileBuf, pBaseName, fileLen);
    }
}

// =====================================================================================================================
// Creates a new directory at the specified path.
Result MkDir(
    const char* pPathName)
{
    int lnxResult = mkdir(pPathName, S_IRWXU);

    Result result = Result::Success;

    if (lnxResult != 0)
    {
        switch (errno)
        {
            case EEXIST:
                result = Result::AlreadyExists;
                break;
            case ENOTDIR:
                result = Result::ErrorInvalidValue;
                break;
            default:
                result = Result::ErrorUnknown;
                break;
        }
    }

    return result;
}

// =====================================================================================================================
// Creates a new directory and all intermediate directories
Result MkDirRecursively(
    const char* pPathName)
{
    char path[PATH_MAX];
    const size_t len = strlen(pPathName);
    Result result = Result::Success;
    int lnxResult;

    Snprintf(path, sizeof(path), "%s", pPathName);

    if (path[len - 1] != '/')
    {
        path[len] = '/';
    }

    for (char* p = path + 1; *p != '\0'; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            lnxResult = mkdir(path, S_IRWXU);
            *p = '/';

            if (lnxResult != 0)
            {
                switch (errno)
                {
                    case EEXIST:
                        result = Result::AlreadyExists;
                        break;
                    case ENOTDIR:
                        result = Result::ErrorInvalidValue;
                        break;
                    default:
                        result = Result::ErrorUnknown;
                        break;
                }
            }
            else
            {
                result = Result::Success;
            }

            if (result != Result::Success && result != Result::AlreadyExists)
            {
                break;
            }
        }
    }

    return result;
}

// =====================================================================================================================
// Lists the contents of the specified directory in an array of strings
Result ListDir(
    const char*  pDirName,
    uint32*      pFileCount,
    const char** ppFileNames,
    size_t*      pBufferSize,
    const void*  pBuffer)
{
    DIR* pDir;
    struct dirent* pEntry;

    Result result = Result::Success;

    if (pDirName == nullptr)
    {
        result = Result::ErrorInvalidPointer;
    }

    if (result == Result::Success)
    {
        pDir = opendir(pDirName);

        if (pDir == nullptr)
        {
            result = Result::ErrorInvalidValue;
        }
    }

    if (result == Result::Success)
    {
        pEntry = readdir(pDir);
        if ((ppFileNames == nullptr) ||
            (pBuffer == nullptr))
        {
            // Obtain file count and buffer size
            uint32 fileCount  = 0;
            size_t bufferSize = 0;

            while (pEntry != nullptr)
            {
                fileCount++;
                bufferSize += strlen(pEntry->d_name);

                pEntry = readdir(pDir);
            }

            *pFileCount  = fileCount;
            *pBufferSize = bufferSize;
        }
        else
        {
            // Populate ppFileNames and pBuffer
            size_t fileNameSize;
            char*  fileName        = (char*)pBuffer;
            uint32 fileIndex       = 0;
            uint32 fileCount       = *pFileCount;
            size_t bufferPopulated = 0;
            size_t bufferSize      = *pBufferSize;

            while ((pEntry != nullptr) &&
                   (fileIndex < fileCount))
            {
                fileNameSize = strlen(pEntry->d_name) + 1;
                bufferPopulated += fileNameSize;

                if (bufferPopulated > bufferSize)
                {
                    break;
                }

                strcpy(fileName, pEntry->d_name);
                ppFileNames[fileIndex] = fileName;
                fileIndex++;
                fileName += fileNameSize;

                pEntry = readdir(pDir);
                fileIndex++;
            }
        }
        closedir(pDir);
    }

    return result;
}

/// Get the Process ID of the current process
uint32 GetIdOfCurrentProcess()
{
    return getpid();
}

// =====================================================================================================================
// Linux-specific wrapper for printing stack trace information.
size_t DumpStackTrace(
    char*   pOutput,
    size_t  bufSize,
    uint32  skipFrames)
{
    PAL_NOT_IMPLEMENTED();
    return 0;
}

} // Util
