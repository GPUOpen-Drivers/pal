/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

// pal
#include "palFile.h"
#include "palSysUtil.h"
#include "palVector.h"
#include "palVectorImpl.h"

// stl
#include <algorithm>
#include <ctime>
#include <thread>

// microsoft

using namespace std::chrono;

namespace Util
{
/// Defines for AMD

/// Defines for Processor Signature
static constexpr uint32 AmdProcessorReserved2        = 0xf0000000;    ///< Bits 31 - 28 of processor signature
static constexpr uint32 AmdProcessorExtendedFamily   = 0x0ff00000;    ///< Bits 27 - 20 of processor signature
static constexpr uint32 AmdProcessorExtendedModel    = 0x000f0000;    ///< Bits 16 - 19 of processor signature
static constexpr uint32 AmdProcessorReserved1        = 0x0000f000;    ///< Bits 15 - 12 of processor signature
static constexpr uint32 AmdProcessorFamily           = 0x00000f00;    ///< Bits 11 -  8 of processor signature
static constexpr uint32 AmdProcessorModel            = 0x000000f0;    ///< Bits  7 -  4 of processor signature
static constexpr uint32 AmdProcessorStepping         = 0x0000000f;    ///< Bits  3 -  0 of processor signature

/// Defines for Instruction Family
static constexpr uint32 AmdInstructionFamily5        = 0x5;           ///< Instruction family 5
static constexpr uint32 AmdInstructionFamily6        = 0x6;           ///< Instruction family 6
static constexpr uint32 AmdInstructionFamilyF        = 0xF;           ///< Instruction family F

/// Defines for Intel

/// Defines for Processor Signature
static constexpr uint32 IntelProcessorExtendedFamily = 0x0ff00000;    ///< Bits 27 - 20 of processor signature
static constexpr uint32 IntelProcessorExtendedModel  = 0x000f0000;    ///< Bits 19 - 16 of processor signature
static constexpr uint32 IntelProcessorType           = 0x00003000;    ///< Bits 13 - 12 of processor signature
static constexpr uint32 IntelProcessorFamily         = 0x00000f00;    ///< Bits 11 -  8 of processor signature
static constexpr uint32 IntelProcessorModel          = 0x000000f0;    ///< Bits  7 -  4 of processor signature
static constexpr uint32 IntelProcessorStepping       = 0x0000000f;    ///< Bits  3 -  0 of processor signature

/// Defines for Family
static constexpr uint32 IntelPentiumFamily           = 0x5;           ///< Any Pentium
static constexpr uint32 IntelP6ArchitectureFamily    = 0x6;           ///< P-III, and some Celeron's
static constexpr uint32 IntelPentium4Family          = 0xF;           ///< Pentium4, Pentium4-M, and some Celeron's

// =====================================================================================================================
// Query cpu type for AMD processor
void QueryAMDCpuType(
    SystemInfo* pSystemInfo)
{
#if PAL_HAS_CPUID
    uint32 reg[4] = {};

    CpuId(reg, 1);

    uint32 model = (reg[0] & AmdProcessorModel) >> 4;
    uint32 extendedModel = (reg[0] & AmdProcessorExtendedModel) >> 16;
    uint32 family = (reg[0] & AmdProcessorFamily) >> 8;
    uint32 extendedFamily = (reg[0] & AmdProcessorExtendedFamily) >> 20;

    uint32 displayModel = model;
    uint32 displayFamily = family;

    switch (family)
    {
        case AmdInstructionFamily5:
            if (model <= 3)
            {
                pSystemInfo->cpuType = CpuType::AmdK5;
            }
            else if (model <= 7)
            {
                pSystemInfo->cpuType = CpuType::AmdK6;
            }
            else if (model == 8)
            {
                pSystemInfo->cpuType = CpuType::AmdK6_2;
            }
            else if (model <= 15)
            {
                pSystemInfo->cpuType = CpuType::AmdK6_3;
            }
            else
            {
                pSystemInfo->cpuType = CpuType::Unknown;
            }
            break;
        case AmdInstructionFamily6: // Athlon
            // All Athlons and Durons fall into this group
            pSystemInfo->cpuType = CpuType::AmdK7;
            break;
        case AmdInstructionFamilyF: // For family F, we must check extended family
            switch (extendedFamily)
            {
                case 0:
                    // All Athlon64s and Opterons fall into this group
                    pSystemInfo->cpuType = CpuType::AmdK8;
                    break;
                case 1:
                    // 'Family 10h' aka K10 aka Barcelona, Phenom, Greyhound
                    pSystemInfo->cpuType = CpuType::AmdK10;
                    break;
                case 3:
                    // Family 12h - Llano
                    pSystemInfo->cpuType = CpuType::AmdFamily12h;
                    break;
                case 5:
                    // ExtFamilyID of '5' for Bobcat as read from ASIC
                    pSystemInfo->cpuType = CpuType::AmdBobcat;
                    break;
                case 6:
                    // Family 15h - Orochi, Trinity, Komodo, Kaveri, Basilisk
                    pSystemInfo->cpuType = CpuType::AmdFamily15h;
                    break;
                case 7:
                    // Family 16h - Kabini
                    pSystemInfo->cpuType = CpuType::AmdFamily16h;
                    break;
                case 8:
                case 10:
                    // Family 17h and Family 19h - Ryzen
                    pSystemInfo->cpuType = CpuType::AmdRyzen;
                    break;
                default:
                    pSystemInfo->cpuType = CpuType::Unknown;
                    break;
            }
            break;
        default:
            pSystemInfo->cpuType = CpuType::Unknown;
            break;
    }

    if (family == AmdInstructionFamilyF)
    {
        // If ExtendedModel[3:0] == 1h and BaseModel[3:0] == 8h, then Model[7:0] = 18h
        displayModel += extendedModel << 4;
        // If BaseFamily[3:0] == Fh and ExtendedFamily[7:0] == 08h, then Family[7:0] = 17h
        displayFamily += extendedFamily;
    }

    pSystemInfo->displayFamily = displayFamily;
    pSystemInfo->displayModel  = displayModel;
#else
    pSystemInfo->cpuType = CpuType::Unknown;
#endif
}

// =====================================================================================================================
// Query cpu type for Intel processor
void QueryIntelCpuType(
    SystemInfo* pSystemInfo)
{
#if PAL_HAS_CPUID
    uint32 reg[4] = {};

    CpuId(reg, 1);

    uint32 model = (reg[0] & IntelProcessorModel) >> 4;
    uint32 extendedModel = (reg[0] & IntelProcessorExtendedModel) >> 16;
    uint32 family = (reg[0] & IntelProcessorFamily) >> 8;
    uint32 extendedFamily = (reg[0] & IntelProcessorExtendedFamily) >> 20;

    uint32 displayModel = model;
    uint32 displayFamily = family;

    switch (family)
    {
        case IntelPentiumFamily:
            pSystemInfo->cpuType = CpuType::IntelOld;
            break;
        case IntelP6ArchitectureFamily:
            {
                switch (model)
                {
                    case 1:
                    case 3:
                    case 5:
                    case 6:
                        pSystemInfo->cpuType = CpuType::IntelOld;
                        break;
                    case 7:
                    case 8:
                    case 11:
                        pSystemInfo->cpuType = CpuType::IntelP3;
                        break;
                    case 9:
                        pSystemInfo->cpuType = CpuType::IntelPMModel9;
                        break;
                    case 13:
                        pSystemInfo->cpuType = CpuType::IntelPMModelD;
                        break;
                    case 14:
                        pSystemInfo->cpuType = CpuType::IntelPMModelE;
                        break;
                    case 15:
                        pSystemInfo->cpuType = CpuType::IntelCoreModelF;
                        break;
                    default:
                        pSystemInfo->cpuType = CpuType::Unknown;
                        break;
                }
            }
            break;
        case IntelPentium4Family:
            // When the family is IntelPentium4, must check the extended family
            switch (extendedFamily)
            {
                case 0:
                    pSystemInfo->cpuType = CpuType::IntelP4;
                    break;
                default:
                    pSystemInfo->cpuType = CpuType::Unknown;
                    break;
            }
            break;
        default:
            pSystemInfo->cpuType = CpuType::Unknown;
            break;
    }

    if (family == IntelPentium4Family)
    {
        displayFamily += extendedFamily;
        displayModel += extendedModel << 4;
    }
    else if (family == IntelP6ArchitectureFamily)
    {
        displayModel += extendedModel << 4;
    }

    pSystemInfo->displayFamily = displayFamily;
    pSystemInfo->displayModel  = displayModel;
#else
    pSystemInfo->cpuType = CpuType::Unknown;
#endif
}

// =====================================================================================================================
// Non-recursively delete the least-recently-accesssed files from a directory until the directory reaches size in bytes.
Result RemoveOldestFilesOfDirUntilSize(
    const char* pPathName,
    uint64      desiredSize)
{
    size_t fileCount = 0;
    size_t bytesReq  = 0;

    // Get the number of files in the dir.
    Result result = CountFilesInDir(pPathName, &fileCount, &bytesReq);

    // If we've already failed, then get out of here.
    // The directory could've been empty, in which case don't do anything and return.
    if ((result != Result::Success) || (fileCount <= 0))
    {
        return result;
    }

    const size_t pathLen = std::strlen(pPathName) + 1; // Add one to append a '/'
    const size_t fullPathSize = pathLen + 1 + Util::MaxFileNameStrLen;

    // Allocate mem for storing file names
    Util::GenericAllocator allocator;
    StringView<char>* pFileNames = static_cast<StringView<char>*>(
        PAL_CALLOC(fileCount * sizeof(StringView<char>), &allocator, AllocInternalTemp));
    char* pFileNameBuffer = static_cast<char*>(PAL_CALLOC(bytesReq, &allocator, AllocInternalTemp));
    char* pFullFilePath = static_cast<char*>(PAL_CALLOC(fullPathSize * sizeof(char), &allocator, AllocInternalTemp));

    if ((pFileNames == nullptr) || (pFileNameBuffer == nullptr) || (pFullFilePath == nullptr))
    {
        result = Result::ErrorOutOfMemory;
    }

    // Get the file names in the dir
    if (result == Result::Success)
    {
        result = GetFileNamesInDir(pPathName, Span(pFileNames, fileCount), Span(pFileNameBuffer, bytesReq));
    }

    // Store the stats of every file in a Vector
    struct Value
    {
        uint32     namePos;
        File::Stat stat;
    };
    Vector<Value, 32, Util::GenericAllocator> files{ &allocator };

    size_t currentSize = 0;
    if (result == Result::Success)
    {
        // Write the path portion of the full file path.
        Strncpy(pFullFilePath, pPathName, fullPathSize);
        Strncpy(pFullFilePath + pathLen - 1, "/", fullPathSize - pathLen + 1);

        for (uint32 i = 0; (i < fileCount) && (result == Result::Success); i++)
        {
            // Write the filename portion of the full file path
            Strncpy(pFullFilePath + pathLen, pFileNames[i].Data(), fullPathSize - pathLen);

            File::Stat stat;
            result = File::GetStat(pFullFilePath, &stat);
            if ((result == Result::Success) && stat.flags.isRegular)
            {
                result = files.PushBack({ i, stat });
                currentSize += stat.size;
            }
        }
    }

    // sort from most-recently-accessed to least-recently-accessed
    if ((result == Result::Success) &&
        // don't bother sorting if we're already under size
        (currentSize > desiredSize))
    {
        // std::sort could throw std::bad_alloc
        // so we're using std::qsort which operates in-place and doesn't throw
        std::qsort(files.Data(), files.size(), sizeof(Value),
            [](const void* vpl, const void* vpr) -> int
            {
                const Value l = *static_cast<const Value*>(vpl);
                const Value r = *static_cast<const Value*>(vpr);

                int ret = 0;
                if (l.stat.atime > r.stat.atime)
                {
                    ret = -1;
                }
                else if (l.stat.atime < r.stat.atime)
                {
                    ret = 1;
                }
                return ret;
            });
    }

    // delete the files
    while ((currentSize > desiredSize) && (files.empty() == false) && (result == Result::Success))
    {
        // Calling Back()/Erase() instead of PopBack() to avoid a deep copy.
        Value* pValue = &files.Back();
        currentSize -= pValue->stat.size;

        Strncpy(pFullFilePath + pathLen, pFileNames[pValue->namePos].Data(), fullPathSize - pathLen);

        result = File::Remove(pFullFilePath);
        files.Erase(pValue);
    }

    PAL_SAFE_FREE(pFileNames, &allocator);
    PAL_SAFE_FREE(pFileNameBuffer, &allocator);
    PAL_SAFE_FREE(pFullFilePath, &allocator);

    return result;
}

// =====================================================================================================================
Result CreateLogDir(
    const char* pBaseDir,
    char*       pLogDir,
    size_t      logDirSize
)
{
    if ((pBaseDir == nullptr) || (pLogDir == nullptr))
    {
        return Result::ErrorInvalidPointer;
    }

    // Try to create the root log directory first, which may already exist.
    const Result tmpResult = MkDir(pBaseDir);
    Result result = (tmpResult == Result::AlreadyExists) ? Result::Success : tmpResult;

    if (result == Result::Success)
    {
        // Even if the dir already exists we try to set permissions in case it was device user who ceated the dir but
        // with not enough permisions.
        result = SetRwxFilePermissions(pBaseDir);
        PAL_ASSERT_MSG(result == Result::Success, "Failed to set main logs directory permissions to RWX for all");
    }

    // Create a directory name that will hold any dumped logs this session.  The name will be composed of the
    // executable name and current date/time, looking something like this: app.exe_2015-08-26_07.49.20.
    // Note that we will append a suffix if some other platform already made this directory in this same second.
    // (Yes, this can actually happen in reality.)
    char  executableNameBuffer[256] = {};
    char* pExecutableName = nullptr;

    if (result == Result::Success)
    {
        result = GetExecutableName(executableNameBuffer, &pExecutableName, sizeof(executableNameBuffer));
    }

    if (result == Result::Success)
    {
        const time_t   rawTime   = time(nullptr);
        const tm*const pTimeInfo = localtime(&rawTime);

        char  dateTimeBuffer[64] = {};
        strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y-%m-%d_%H.%M.%S", pTimeInfo);
        Snprintf(pLogDir, logDirSize, "%s/%s_%s", pBaseDir, pExecutableName, dateTimeBuffer);

        // Try to create the directory. If it already exists, keep incrementing the suffix until it works.
        const size_t suffixOffset = strlen(pLogDir);
        uint32       suffix       = 0;

        do
        {
            Snprintf(pLogDir + suffixOffset, logDirSize - suffixOffset, "_%02d", suffix++);
            result = MkDir(pLogDir);
        }
        while (result == Result::AlreadyExists);
    }

    return result;
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION < 863
// These have to be defined in a .cpp file because using any sort of a min()/max() function can conflict with clients
// still using Microsoft's min()/max() macros.

// =====================================================================================================================
Result RemoveFilesOfDirOlderThan(
    const char* pPathName,
    uint64      thresholdSeconds)
{
    return RemoveFilesOfDirOlderThan(
        pPathName,
        SecondsSinceEpoch{ seconds{ std::min(thresholdSeconds, uint64(seconds::max().count())) } });
}

// =====================================================================================================================
Result GetStatusOfDir(
    const char* pPathName,
    uint64*     pTotalSize,
    uint64*     pOldestTime)
{
    SecondsSinceEpoch oldestTime;
    const Result ret = GetStatusOfDir(pPathName, pTotalSize, &oldestTime);
    *pOldestTime = oldestTime.time_since_epoch().count();
    return ret;
}
#endif

// =====================================================================================================================
void Sleep(
    milliseconds duration)
{
    std::this_thread::sleep_for(duration);
}

#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 866
#endif
} // Util
