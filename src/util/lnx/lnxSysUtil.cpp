/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

constexpr KeyCode KeyLookupTable[] =
{
    KeyCode::Undefined,
    KeyCode::Esc,        // KEY_ESC              = 1
    KeyCode::One,        // KEY_1                = 2
    KeyCode::Two,        // KEY_2                = 3
    KeyCode::Three,      // KEY_3                = 4
    KeyCode::Four,       // KEY_4                = 5
    KeyCode::Five,       // KEY_5                = 6
    KeyCode::Six,        // KEY_6                = 7
    KeyCode::Seven,      // KEY_7                = 8
    KeyCode::Eight,      // KEY_8                = 9
    KeyCode::Nine,       // KEY_9                = 10
    KeyCode::Zero,       // KEY_0                = 11
    KeyCode::Minus,      // KEY_MINUS            = 12
    KeyCode::Equal,      // KEY_EQUAL            = 13
    KeyCode::Backspace,  // KEY_BACKSPACE        = 14
    KeyCode::Tab,        // KEY_TAB              = 15
    KeyCode::Q,          // KEY_Q                = 16
    KeyCode::W,          // KEY_W                = 17
    KeyCode::E,          // KEY_E                = 18
    KeyCode::R,          // KEY_R                = 19
    KeyCode::T,          // KEY_T                = 20
    KeyCode::Y,          // KEY_Y                = 21
    KeyCode::U,          // KEY_U                = 22
    KeyCode::I,          // KEY_I                = 23
    KeyCode::O,          // KEY_O                = 24
    KeyCode::P,          // KEY_P                = 25
    KeyCode::LBrace,     // KEY_LEFTBRACE        = 26
    KeyCode::RBrace,     // KEY_RIGHTBRACE       = 27
    KeyCode::Enter,      // KEY_ENTER            = 28
    KeyCode::LControl,   // KEY_LEFTCTRL         = 29
    KeyCode::A,          // KEY_A                = 30
    KeyCode::S,          // KEY_S                = 31
    KeyCode::D,          // KEY_D                = 32
    KeyCode::F,          // KEY_F                = 33
    KeyCode::G,          // KEY_G                = 34
    KeyCode::H,          // KEY_H                = 35
    KeyCode::J,          // KEY_J                = 36
    KeyCode::K,          // KEY_K                = 37
    KeyCode::L,          // KEY_L                = 38
    KeyCode::Semicolon,  // KEY_SEMICOLON        = 39
    KeyCode::Apostrophe, // KEY_APOSTROPHE       = 40
    KeyCode::Backtick,   // KEY_GRAVE            = 41
    KeyCode::LShift,     // KEY_LEFTSHIFT        = 42
    KeyCode::Backslash,  // KEY_BACKSLASH        = 43
    KeyCode::Z,          // KEY_Z                = 44
    KeyCode::X,          // KEY_X                = 45
    KeyCode::C,          // KEY_C                = 46
    KeyCode::V,          // KEY_V                = 47
    KeyCode::B,          // KEY_B                = 48
    KeyCode::N,          // KEY_N                = 49
    KeyCode::M,          // KEY_M                = 50
    KeyCode::Comma,      // KEY_COMMA            = 51
    KeyCode::Dot,        // KEY_DOT              = 52
    KeyCode::Slash,      // KEY_SLASH            = 53
    KeyCode::RShift,     // KEY_RIGHTSHIFT       = 54
    KeyCode::NumAsterisk,// KEY_KPASTERISK       = 55
    KeyCode::LAlt,       // KEY_LEFTALT          = 56
    KeyCode::Space,      // KEY_SPACE            = 57
    KeyCode::Capslock,   // KEY_CAPSLOCK         = 58
    KeyCode::F1,         // KEY_F1               = 59
    KeyCode::F2,         // KEY_F2               = 60
    KeyCode::F3,         // KEY_F3               = 61
    KeyCode::F4,         // KEY_F4               = 62
    KeyCode::F5,         // KEY_F5               = 63
    KeyCode::F6,         // KEY_F6               = 64
    KeyCode::F7,         // KEY_F7               = 65
    KeyCode::F8,         // KEY_F8               = 66
    KeyCode::F9,         // KEY_F9               = 67
    KeyCode::F10,        // KEY_F10              = 68
    KeyCode::Numlock,    // KEY_NUMLOCK          = 69
    KeyCode::Scroll,     // KEY_SCROLLLOCK       = 70
    KeyCode::Num7,       // KEY_KP7              = 71
    KeyCode::Num8,       // KEY_KP8              = 72
    KeyCode::Num9,       // KEY_KP9              = 73
    KeyCode::NumMinus,   // KEY_KPMINUS          = 74
    KeyCode::Num4,       // KEY_KP4              = 75
    KeyCode::Num5,       // KEY_KP5              = 76
    KeyCode::Num6,       // KEY_KP6              = 77
    KeyCode::NumPlus,    // KEY_KPPLUS           = 78
    KeyCode::Num1,       // KEY_KP1              = 79
    KeyCode::Num2,       // KEY_KP2              = 80
    KeyCode::Num3,       // KEY_KP3              = 81
    KeyCode::Num0,       // KEY_KP0              = 82
    KeyCode::NumDot,     // KEY_KPDOT            = 83
    KeyCode::Undefined,  // 84
    KeyCode::Undefined,  // KEY_ZENKAKUHANKAKU   = 85
    KeyCode::Undefined,  // KEY_102ND            = 86
    KeyCode::F11,        // KEY_F11              = 87
    KeyCode::F12,        // KEY_F12              = 88
    KeyCode::Undefined,  // KEY_RO               = 89
    KeyCode::Undefined,  // KEY_KATAKANA         = 90
    KeyCode::Undefined,  // KEY_HIRAGANA         = 91
    KeyCode::Undefined,  // KEY_HENKAN           = 92
    KeyCode::Undefined,  // KEY_KATAKANAHIRAGANA = 93
    KeyCode::Undefined,  // KEY_MUHENKAN         = 94
    KeyCode::Undefined,  // KEY_KPJPCOMMA        = 95
    KeyCode::NumEnter,   // KEY_KPENTER          = 96
    KeyCode::RControl,   // KEY_RIGHTCTRL        = 97
    KeyCode::NumSlash,   // KEY_KPSLASH          = 98
    KeyCode::Undefined,  // KEY_SYSRQ            = 99
    KeyCode::RAlt,       // KEY_RIGHTALT         = 100
    KeyCode::Undefined,  // KEY_LINEFEED         = 101
    KeyCode::Home,       // KEY_HOME             = 102
    KeyCode::ArrowUp,    // KEY_UP               = 103
    KeyCode::PageUp,     // KEY_PAGEUP           = 104
    KeyCode::ArrowLeft,  // KEY_LEFT             = 105
    KeyCode::ArrowRight, // KEY_RIGHT            = 106
    KeyCode::End,        // KEY_END              = 107
    KeyCode::ArrowDown,  // KEY_DOWN             = 108
    KeyCode::PageDown,   // KEY_PAGEDOWN         = 109
    KeyCode::Insert,     // KEY_INSERT           = 110
    KeyCode::Delete      // KEY_DELETE           = 111
};

static_assert(KeyLookupTable[KEY_BACKSLASH] == KeyCode::Backslash, "Wrong KeyLookupTable");
static_assert(KeyLookupTable[KEY_DELETE]    == KeyCode::Delete,    "Wrong KeyLookupTable");

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
        pSystemInfo->cpuFrequency = 0;

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
static bool CompareKeys(
    KeyCode keyToCheck,
    KeyCode keyReceived)
{
    bool result = (keyToCheck == keyReceived);
    if (!result)
    {
        if (keyToCheck == KeyCode::Shift)
        {
            result = ((keyReceived == KeyCode::LShift) ||
                      (keyReceived == KeyCode::RShift));
        }
        else if (keyToCheck == KeyCode::Control)
        {
            result = ((keyReceived == KeyCode::LControl) ||
                      (keyReceived == KeyCode::RControl));
        }
        else if (keyToCheck == KeyCode::Alt)
        {
            result = ((keyReceived == KeyCode::LAlt) ||
                      (keyReceived == KeyCode::RAlt));
        }
    }
    return result;
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
                if (CompareKeys(keys[index], keyGet))
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
#if defined(PAL_SHORT_WCHAR)
        Mbstowcs(pWcBuffer, buffer, count);
#else
        // According to MSDN, if mbstowcs encounters an invalid multibyte character, it returns -1
        int success = (int)mbstowcs(pWcBuffer, static_cast<char *>(buffer), count);
        PAL_ASSERT(success > 0);
#endif

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

// =====================================================================================================================
void BeepSound(
    uint32 frequency,
    uint32 duration)
{
    PAL_NOT_IMPLEMENTED();
}

} // Util
