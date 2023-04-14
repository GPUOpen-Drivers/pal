/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palFile.h"
#include "palSysMemory.h"
#include "palMemTrackerImpl.h"
#include "palHashMapImpl.h"
#include "palUuid.h"
#include "lnxSysUtil.h"
#include <mutex>

#include <cwchar>
#include <errno.h>
#include <libgen.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <poll.h>
#include <link.h>

namespace Util
{

#if PAL_HAS_CPUID
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

    // According to spec, CPUID with EAX=0x8000001D, ECX=0x03
    // CPUID_Fn8000001D_EAX_x03 [Cache Properties (L3)] (CachePropEax3)
    // Bits(25:14)  NumSharingCache: number of logical processors sharing this cache.
    //              The number of logical processors sharing this cache is NumSharingCache+1.
    uint32 reg[4] = {0, 0, 0, 0};
    CpuId(reg, 0x8000001D, 0x3);

    uint32 numSharingCache = ((reg[0] >> 14) & 0xfff) + 1;
    uint32 logicalProcessorMask = (1 << numSharingCache) - 1;
    maxCount = Min(maxCount, totalLogicalCoreCount / numSharingCache);

    // each CCX has a mask like these: 0xff, 0xff00, ...
    while (idx < maxCount)
    {
        pData[idx] = logicalProcessorMask;
        idx++;
        logicalProcessorMask = logicalProcessorMask << numSharingCache;
    }

    return Result::Success;
}
#endif

// =====================================================================================================================
// Queries system information.
Result QuerySystemInfo(
    SystemInfo* pSystemInfo)
{
    Result result = Result::ErrorInvalidPointer;

    if (pSystemInfo != nullptr)
    {
        // Null-terminate the string. The returned vendor string is always 12 bytes and does not include a terminator.
        pSystemInfo->cpuVendorString[12] = '\0';
#if PAL_HAS_CPUID
        uint32 regValues[4] = {};

        // Query the vendor string
        CpuId(regValues, 0);

        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[0]) = regValues[1]; // EBX
        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[4]) = regValues[3]; // EDX
        *reinterpret_cast<uint32*>(&pSystemInfo->cpuVendorString[8]) = regValues[2]; // ECX

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
#else
        // Non-x86 platforms don't have an unprivileged cpuid intrinisic, so get that info from the OS
        result = Result::Success;
#endif

        pSystemInfo->cpuLogicalCoreCount  = 0;
        pSystemInfo->cpuPhysicalCoreCount = 0;
        pSystemInfo->cpuFrequency = 0;
        uint32 cpuClockSpeedTotal = 0;

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
            typedef Util::HashMap<uint32, CpuCoreCount, GenericAllocatorTracked> PhysicalPackageCoreCountMap;

            constexpr size_t        BufSize            = 8*1024;
            constexpr uint32        MaxSocketsHint     = 4;
            CpuCoreCount*           pCoreCount         = nullptr;
            bool                    coreCountPopulated = false;
            GenericAllocatorTracked allocator;

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
            }
        }

        cpuInfoFile.Close();

#if PAL_HAS_CPUID
        // GetCcxMask() should be called only for Ryzen for now.
        if ((result == Result::Success) && (pSystemInfo->cpuType == CpuType::AmdRyzen))
        {
            result = GetCcxMask(pSystemInfo, pSystemInfo->cpuLogicalCoreCount);
        }
#endif

        if (result == Result::Success)
        {
             uint64 totalMemByteSize = sysconf( _SC_PHYS_PAGES ) * sysconf( _SC_PAGESIZE );
             pSystemInfo->totalSysMemSize = static_cast<uint32>(totalMemByteSize / 1024 / 1024);

             // /proc/cpuinfo varies by arch, so we might have got no data at all.
             // Have a robust but less-detailed fallback
             if (pSystemInfo->cpuPhysicalCoreCount == 0)
             {
                 pSystemInfo->cpuPhysicalCoreCount = sysconf(_SC_NPROCESSORS_ONLN);
             }
             if (pSystemInfo->cpuLogicalCoreCount == 0)
             {
                 pSystemInfo->cpuLogicalCoreCount = pSystemInfo->cpuPhysicalCoreCount;

             }
             pSystemInfo->cpuFrequency = cpuClockSpeedTotal / pSystemInfo->cpuLogicalCoreCount;
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
int64 GetPerfCpuTime(
    bool raw)
{
    int64 time = 0LL;

    // clock_gettime() returns the monotonic time since EPOCH
    timespec ts = { };
    if (clock_gettime(raw ? CLOCK_MONOTONIC_RAW : CLOCK_MONOTONIC, &ts) == 0)
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
    bool    ret = false;

    PAL_ASSERT(pCode != nullptr);

    if ((input < static_cast<int>(ArrayLen(KeyLookupTable))) && (input >= 0))
    {
        if (KeyLookupTable[input] != KeyCode::Undefined)
        {
            *pCode = KeyLookupTable[input];
            ret    = true;
        }
    }

    return ret;
}

// =====================================================================================================================
static uint32 FindKeyboardDeviceNodes(
    int32 (&keyboards)[MaxKeyboards])
{
    struct dirent** ppNameList                = nullptr;
    const char     *path                      = "/dev/input/by-path/";
    char            kbdPath[MaxPathStrWidth]  = { 0 };
    char            nodeName[MaxPathStrWidth] = { 0 };
    uint32          kbdIdx                    = 0; // 'keyboard index', to track up to MaxKeyboards keyboards

    // initialize keyboards to InvalidFd (-1) default val
    for (uint32 i = 0; i < MaxKeyboards; i++)
    {
        keyboards[i] = InvalidFd;
    }

    // iterate the files on the directory
    const int32 numDirs = scandir(path, &ppNameList, 0, alphasort);

    for (int32 dirIdx = 0; dirIdx < numDirs; dirIdx++)
    {
        // Note: 'kbd' is the key word that refers to the keyboard.
        // Note: it is still required to iterate all ppNameList to free memory in case the node has been found
        // Note: this function looks for up to MaxKeyboards and will only set valid file descriptors

        if ((kbdIdx < MaxKeyboards) && (strstr(ppNameList[dirIdx]->d_name, "kbd")))
        {
            // reset kbdPath and nodeName to original path when looking for new keyboard
            strcpy(kbdPath, path);
            strcpy(nodeName, path);
            // construct the absolute path for the soft link
            strcat(kbdPath, ppNameList[dirIdx]->d_name);
            char linkName[64] = { 0 };
            // get the relative path for the keyboard's device node
            int32 n = readlink(kbdPath, linkName, sizeof(linkName));
            if (n > 0)
            {
                strncat(nodeName, linkName, n);
                const int fd = open(nodeName, O_RDONLY | O_NONBLOCK);
                if (fd != InvalidFd)
                {
                    keyboards[kbdIdx] = fd;
                    kbdIdx++;
                }
            }
        }
        free(ppNameList[dirIdx]);
    }

    if (ppNameList)
    {
        free(ppNameList);
    }

    return kbdIdx;
}

// =====================================================================================================================
// Reports whether the specified key has been pressed down.
bool IsKeyPressed(
    KeyCode key,
    bool*   pPrevState)
{
    static KeyBitset keyBitset;
    // We need to lock the scope since we're dealing with multiple keyboards setting keyBitset across multiple threads
    MutexAuto lock(keyBitset.GetKeyBitsetLock());

    static int32  keyboards[MaxKeyboards];
    static uint32 numKeyboards = FindKeyboardDeviceNodes(keyboards);

    static KeyCode keys[2] = {};
    bool isKeySet          = false;
    int  maxIndex          = IsComboKey(key, keys) ? 1 : 0;

    struct input_event ev = {};

    for (uint32 kbdIdx = 0; kbdIdx < numKeyboards; kbdIdx++)
    {
        int32 retVal = 0;

        // Process outstanding key events and updated the key bitmap.
        while ((retVal >= 0) && (keyboards[kbdIdx] != InvalidFd))
        {
            retVal = read(keyboards[kbdIdx], &ev, sizeof(ev));

            if ((retVal >= 0) &&   // The read returned an event
                (ev.type == 1))    // The event is EV_KEY
            {
                KeyCode keyGet;
                if (KeyTranslate(ev.code, &keyGet))
                {
                    if (ev.value == 0)
                    {
                        // 0: key release
                        keyBitset.Clear(keyGet);
                    }
                    else if (ev.value == 1)
                    {
                        // 1: key pressed
                        keyBitset.Set(keyGet);
                    }
                }
            }
            else if ((retVal == -1) &&
                     (errno != EAGAIN))
            {
                // If errno is not EAGAIN, close this device if it was open
                close(keyboards[kbdIdx]);
                keyboards[kbdIdx] = InvalidFd;
            }
        }
    }

    for (int i = 0; i <= maxIndex; i++)
    {
        isKeySet = keyBitset.Test(keys[i]);
        if (isKeySet == false)
        {
            break;
        }
    }

    if (pPrevState != nullptr)
    {
        if (isKeySet && (*pPrevState == false))
        {
            isKeySet    = true;
            *pPrevState = true;
        }
        else
        {
            if (isKeySet == false)
            {
                *pPrevState = false;
            }

            isKeySet = false;
        }
    }

    return isKeySet;
}

// =====================================================================================================================
// Determines if profiling is restricted
bool IsProfileRestricted()
{
    bool result = false;

    char buffer[2];

    // Attempt to determine whether the process is debuggable by checking the contents of /proc/self/debuggable.
    if (readlink("/proc/self/debuggable", &buffer[0], (sizeof(buffer) - sizeof(char))) >= 0)
    {
        // Check whether there will be 0 or 1.
        // Set connection unavailable when content is '0'.
        result = static_cast<int32>(buffer[0]) - 48;
    }
    else
    {
        PAL_ALERT_ALWAYS();
        result = false;
    }

    return result;
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

    const ssize_t count = readlink("/proc/self/exe", pBuffer, bufferLength);
    if ((count >= 0) && (static_cast<size_t>(count) < bufferLength))
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

    const ssize_t count = readlink("/proc/self/exe", &buffer[0], sizeof(buffer));

    // Convert to wide char
    if ((count >= 0) &&
        (static_cast<size_t>(count) < sizeof(buffer)) &&
        (static_cast<size_t>(count) < bufferLength))
    {
        buffer[count] = '\0';
        Mbstowcs(pWcBuffer, buffer, bufferLength);

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

    wchar_t*const pLastSlash = Wcsrchr(pWcBuffer, L'/');
    (*ppWcFilename) = (pLastSlash == nullptr) ? pWcBuffer : (pLastSlash + 1);

    return result;
}

// =====================================================================================================================
// Gets the current library name. ie: the name of the library containing the function'GetCurrentLibraryName'.
// Optionally, it will also return the extension if the input buffer for extension is valid.
Result GetCurrentLibraryName(
    char*  pLibBuffer,
    size_t libBufferLength,
    char*  pExtBuffer,
    size_t extBufferLength)
{
    Result result = Result::Success;
    PAL_NOT_IMPLEMENTED(); // todo: dladdr and splitpath
    return result;
}

// =====================================================================================================================
struct BuildIdCbData {
    BuildId*    pBuildId; // out
    const void* pLibBase; // target library to search for
};

// =====================================================================================================================
int BuildIdEachLibCallback(
    struct dl_phdr_info* pInfo,
    size_t               size,
    void*                pData)
{
    int retval = 0; // found = 0
    BuildIdCbData* pCbData = reinterpret_cast<BuildIdCbData*>(pData);

    // Check if we're in the right library by looking at the first loaded segment.
    void* pLibBase = nullptr;
    for (uint32 i = 0; i < pInfo->dlpi_phnum; i++)
    {
        if (pInfo->dlpi_phdr[i].p_type == PT_LOAD)
        {
            pLibBase = reinterpret_cast<void*>(pInfo->dlpi_addr + pInfo->dlpi_phdr[i].p_vaddr);
            break;
        }
    }

    if (pLibBase == pCbData->pLibBase)
    {
        // We're in the right library; now look for notes!
        for (uint32 i = 0; i < pInfo->dlpi_phnum; i++)
        {
            if (pInfo->dlpi_phdr[i].p_type == PT_NOTE)
            {
                void* pElfNote = reinterpret_cast<void*>(pInfo->dlpi_addr + pInfo->dlpi_phdr[i].p_vaddr);
                ssize_t elfSecLen = pInfo->dlpi_phdr[i].p_memsz;
                while (elfSecLen > 0)
                {
                    // Note sections may have one or more notes. Keep at it till we run out of bytes
                    PAL_ASSERT(elfSecLen >= sizeof(ElfW(Nhdr)));
                    ElfW(Nhdr)* pElfNoteHdr = reinterpret_cast<ElfW(Nhdr)*>(pElfNote);
                    size_t noteTotalSize = (sizeof(ElfW(Nhdr)) +
                                            Pow2Align(pElfNoteHdr->n_namesz, 4) +
                                            Pow2Align(pElfNoteHdr->n_descsz, 4));
                    PAL_ASSERT(elfSecLen >= noteTotalSize);

                    char* pElfNoteName = reinterpret_cast<char*>(VoidPtrInc(pElfNote, sizeof(ElfW(Nhdr))));
                    char* pElfNoteDesc = reinterpret_cast<char*>(
                        VoidPtrInc(pElfNote, sizeof(ElfW(Nhdr)) + Pow2Align(pElfNoteHdr->n_namesz, 4))
                        );

                    if ((pElfNoteHdr->n_type == NT_GNU_BUILD_ID) &&
                        (pElfNoteHdr->n_namesz == strlen(ELF_NOTE_GNU) + 1) &&
                        (strncmp(pElfNoteName, ELF_NOTE_GNU, pElfNoteHdr->n_namesz) == 0))
                    {
                        // Found the note! Copy it out.
                        memcpy(pCbData->pBuildId,
                                pElfNoteDesc,
                                Util::Min(sizeof(BuildId), static_cast<size_t>(pElfNoteHdr->n_descsz)));
                        retval = 1; // found!
                        break;
                    }

                    pElfNote   = VoidPtrInc(pElfNote, noteTotalSize);
                    elfSecLen -= noteTotalSize;
                }
            }
        }
    }
    return retval;
};

// =====================================================================================================================
// Get a build id for the code at the specified address, without caching or watertight fallbacks.
static Result GetLibFileBuildId(
    BuildId* pBuildId,
    const void* pCodeAddr)
{
    Result result = Result::Success;
    bool found = false;

    Dl_info libInfo = {};
    if (dladdr(pCodeAddr, &libInfo) == 0)
    {
        result = Result::ErrorUnknown;
    }

    if (result == Result::Success)
    {
        // Let's parse the currently-running library and look for a build id
        // This is still opt-in and pretty rare on linux, sadly, but when it is available, it's the
        // best way to get this info race-free besides taking a full checksum of the whole library
        // at runtime.
        BuildIdCbData callbackData = { pBuildId, libInfo.dli_fbase};
        found = dl_iterate_phdr(BuildIdEachLibCallback, &callbackData);
    }

    if ((result == Result::Success) && (found == false))
    {
        // No built in ID but found the library. We'll use the file timestamp as something "good enough"
        // Caveats:
        //  - This can fail if someone deleted the current library. Unfixable with just the file.
        //  - Folks can change this at will by modifying raw file timestamps. Unfixable with just the file.
        //  - Copying a new driver can cause a race condition between the time it was loaded and when we
        //    check the time. To address this, reject any timestamps newer than the process start time.
        File::Stat procStat;
        File::Stat dllStat;
        result = File::GetStat("/proc/self", &procStat);

        if (result == Result::Success)
        {
            result = File::GetStat(libInfo.dli_fname, &dllStat);
        }
        if ((result == Result::Success) && (procStat.mtime >= dllStat.mtime))
        {
            found = true;
            static_assert(sizeof(dllStat.mtime) <= sizeof(pBuildId->data), "Build ID is too small!");
            PAL_ASSERT(dllStat.mtime != 0);
            memcpy(&pBuildId->data[0], &dllStat.mtime, sizeof(dllStat.mtime));
        }
    }

    if ((result == Result::Success) && (found == false))
    {
        result = Result::ErrorUnavailable;
    }

    return result;
}

// =====================================================================================================================
// Gets a unique ID for the current library or else returns a random hash
bool GetCurrentLibraryBuildId(
    BuildId* pBuildId)
{
    static bool           s_buildIdPersists = true;
    static BuildId        s_buildId = {};
    static std::once_flag s_buildIdInit;

    std::call_once(s_buildIdInit, [](BuildId* pInnerBuildId, bool* pPersists)
        {
            memset(pInnerBuildId->data, 0, sizeof(pBuildId->data));
            Result result = GetLibFileBuildId(pInnerBuildId, reinterpret_cast<void*>(GetCurrentLibraryBuildId));

            if (result != Result::Success)
            {
                static_assert(sizeof(pInnerBuildId->data) <= sizeof(Uuid::Uuid), "Build ID is too small!");
                *pPersists = false;
                Uuid::Uuid randUuid = Uuid::Uuid4();
                memcpy(pInnerBuildId->data, randUuid.raw, sizeof(randUuid.raw));
            }
        },
        &s_buildId,
        &s_buildIdPersists
    );

    *pBuildId = s_buildId;
    return s_buildIdPersists;
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
        result = ConvertErrno(errno);
    }

    return result;
}

// =====================================================================================================================
// Creates a new directory and all intermediate directories
Result MkDirRecursively(
    const char* pPathName)
{
    char path[PATH_MAX];
    Result result                 = Result::AlreadyExists;
    constexpr char Separator      = '/';
    constexpr char NullTerminator = '\0';

    // - 1 to leave room to insert possible trailing slash
    Strncpy(path, pPathName, sizeof(path) - 1);

    const size_t len = strlen(path);
    if (len > 0)
    {
        // make sure the last character is a slash to trigger creation attempt
        if (path[len - 1] != Separator)
        {
            path[len]     = Separator;
            path[len + 1] = NullTerminator;
        }

        for (char* p = &path[1]; *p != NullTerminator; p++)
        {
            if (*p == Separator)
            {
                *p     = NullTerminator;
                result = MkDir(path);
                *p     = Separator;

                if ((result != Result::Success) && (result != Result::AlreadyExists))
                {
                    break;
                }
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

// =====================================================================================================================
// Remove files of a directory at the specified path when file time < threshold.
static Result RmDir(
    const char* pDirParentPath,
    uint64      threshold)
{
    Result result = Result::Success;

    DIR* pDir = opendir(pDirParentPath);
    if (pDir ==  nullptr)
    {
        result = Result::ErrorUnknown;
    }

    if (result == Result::Success)
    {
        struct dirent* pEntry   = nullptr;
        struct stat st          = {};

        while ((pEntry = readdir(pDir)) != nullptr)
        {
            if ((strcmp(pEntry->d_name, ".") == 0) || (strcmp(pEntry->d_name, "..") == 0))
            {
                continue;
            }

            char subPath[PATH_MAX];

            Strncpy(subPath, pDirParentPath, PATH_MAX);
            Strncat(subPath, PATH_MAX, "/");
            Strncat(subPath, PATH_MAX, pEntry->d_name);
            if (lstat(subPath, &st) != 0)
            {
                continue;
            }
            if (S_ISDIR(st.st_mode))
            {
                result = RmDir(subPath, threshold);
                if (result != Result::Success)
                {
                    break;
                }
                rmdir(subPath);
            }
            else if (S_ISREG(st.st_mode))
            {
                uint64 fileTime = Max(st.st_atime, st.st_mtime);
                if (fileTime < threshold)
                {
                    unlink(subPath);
                }
            }
            else
            {
                continue;
            }
        }
        closedir(pDir);
    }

    return result;
}

// =====================================================================================================================
// Remove all files below threshold of a directory at the specified path.
Result RemoveFilesOfDir(
    const char* pPathName,
    uint64      threshold)
{
    struct stat st = {};
    Result result  = Result::Success;

    if (lstat(pPathName, &st) != 0)
    {
        result = Result::ErrorInvalidValue;
    }

    if ((result == Result::Success) && S_ISDIR(st.st_mode))
    {
        if ((strcmp(pPathName, ".") == 0) || (strcmp(pPathName, "..") == 0))
        {
            result = Result::ErrorInvalidValue;
        }
        else
        {
            result = RmDir(pPathName, threshold);
        }
    }

    return result;
}

// =====================================================================================================================
// Get status of a directory at the specified path.
Result GetStatusOfDir(
    const char* pPathName,
    uint64*     pTotalSize,
    uint64*     pOldestTime)
{
    DIR* pDir               = nullptr;
    struct dirent* pEntry   = nullptr;
    struct stat statBuf     = {};
    Result result           = Result::Success;

    if ((pDir = opendir(pPathName)) == nullptr)
    {
        result = Result::ErrorUnknown;
    }

    if (result == Result::Success)
    {
        while ((pEntry = readdir(pDir)) != nullptr)
        {
            char subPath[PATH_MAX];

            Strncpy(subPath, pPathName, PATH_MAX);
            Strncat(subPath, PATH_MAX, "/");
            Strncat(subPath, PATH_MAX, pEntry->d_name);
            lstat(subPath, &statBuf);
            if (S_ISDIR(statBuf.st_mode))
            {
                if ((strcmp(".", pEntry->d_name) == 0) || (strcmp("..", pEntry->d_name) == 0))
                {
                    continue;
                }

                *pTotalSize += statBuf.st_size;
                result = GetStatusOfDir(subPath, pTotalSize, pOldestTime);
                if (result != Result::Success)
                {
                    break;
                }
            }
            else
            {
                *pTotalSize += statBuf.st_size;
                uint64 fileTime = Max(statBuf.st_atime, statBuf.st_mtime);
                *pOldestTime = *pOldestTime == 0 ? fileTime : Min(*pOldestTime, fileTime);
            }
        }

        closedir(pDir);
    }

    return result;
}

// =====================================================================================================================
// Almost-Posix-style rename file or directory: replaces already-existing file.
// Posix says this operation is atomic; Windows does not specify.
Result Rename(
    const char* pOldName,
    const char* pNewName)
{
    return (rename(pOldName, pNewName) == 0) ?  Result::Success : Result::ErrorInvalidValue;
}

// =====================================================================================================================
// Get the Process ID of the current process
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

// =====================================================================================================================
void SleepMs(
    uint32 duration)
{
    constexpr uint32 MsPerSec = 1000;
    constexpr uint32 NsPerMs  = (1000 * 1000);

    struct timespec timeRemaining = { };
    struct timespec timeToSleep = { };
    timeToSleep.tv_sec  = (duration / MsPerSec);
    timeToSleep.tv_nsec = ((duration % MsPerSec) * NsPerMs);

    while (true)
    {
        if (::nanosleep(&timeToSleep, &timeRemaining) == 0)
        {
            break; // Successfully slept for the requested duration.
        }
        else if (errno == EINTR)
        {
            // A signal has interrupted the call to nanosleep.  timeRemaining has the amount of remaining time from
            // the original duration.
            timeToSleep = timeRemaining;
            // Try again on the next iteration through the loop...
        }
        else
        {
            PAL_ALERT_ALWAYS_MSG("Unexpected error from nanosleep().");
            break;
        }
    } // while (true)
}

// =====================================================================================================================
void BeepSound(
    uint32 frequency,
    uint32 duration)
{
    PAL_NOT_IMPLEMENTED();
}

// =====================================================================================================================
bool IsDebuggerAttached()
{
    File procFile     = {};
    char lineBuf[128] = {};
    bool isAttached   = false; // If anything at all goes wrong, return false

    Result result = procFile.Open("/proc/self/status", FileAccessRead);
    while (result == Result::Success)
    {
        result = procFile.ReadLine(lineBuf, sizeof(lineBuf), nullptr);
        if (result == Result::Success)
        {
            if (strstr(lineBuf, "TracePid") != nullptr)
            {
                // Eg, "TracePid:     0"
                // If this is non-zero, a process (debugger) is using ptrace on us
                const char* pPidStr = strpbrk(lineBuf, "0123456789");
                if (pPidStr != nullptr)
                {
                    isAttached = (atoi(pPidStr) != 0);
                }
                break;
            }
        }
    }
    return isAttached;
}

// =====================================================================================================================
// Set path to be accessible by everyone.
Result SetRwxFilePermissions(
    const char* pFileName)
{
    Result result = Result::Success;
    struct stat info;
    int ret = stat(pFileName, &info);

    if (ret == -1)
    {
        PAL_ALERT_ALWAYS_MSG("Failed to get stats for %s: %d - %s", pFileName, errno, strerror(errno));
        result = Result::ErrorUnknown;
    }
    else if ((info.st_mode & (ACCESSPERMS)) != (ACCESSPERMS))
    {
        const uid_t euid = geteuid();
        if (info.st_uid != euid)
        {
            PAL_ALERT_ALWAYS_MSG(
                "Failed to set user access permission for %s due to mismach between owner and user ID", pFileName);
            result = Result::ErrorUnknown;
        }

        if (result == Result::Success)
        {
            ret = chmod(pFileName, ACCESSPERMS);
            if (ret == -1)
            {
                PAL_ALERT_ALWAYS_MSG(
                    "Failed to set user access permission for %s: %d - %s", pFileName, errno, strerror(errno));
                result = Result::ErrorUnknown;
            }
        }
    }

    return result;
}
} // Util
