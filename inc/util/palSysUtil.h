/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

/**
 ***********************************************************************************************************************
 * @file  palSysUtil.h
 * @brief PAL utility collection system functions.
 ***********************************************************************************************************************
 */

#pragma once

#include "palUtil.h"
#include <atomic>

#include <cpuid.h>

namespace Util
{

static constexpr uint32 RyzenMaxCcxCount = 4;
static constexpr uint32 CpuVendorAmd     = 0x01000000;
static constexpr uint32 CpuVendorIntel   = 0x02000000;

/// Specifies a keyboard key for detecting key presses.
enum class KeyCode : uint32
{
    F10,
    F11,
    F12,
    Shift,
};

/// Enum to identify possible configurations
enum class CpuType : uint32
{
    Unknown         = 0,                       ///< No capabilites set
    AmdK5           = (CpuVendorAmd + 0),      ///< No MMX, no cmov, no 3DNow
    AmdK6           = (CpuVendorAmd + 1),      ///< No MMX, no cmov, 3DNow (models 6 and 7)
    AmdK6_2         = (CpuVendorAmd + 2),      ///< MMX, no cmov, 3DNow (model 8, no HW WC but not part of cpuid)
    AmdK6_3         = (CpuVendorAmd + 3),      ///< MMX, no cmov, 3DNow (model 9)
    AmdK7Basic      = (CpuVendorAmd + 4),      ///< K7 missing one of the features of K7
    AmdK7           = (CpuVendorAmd + 5),      ///< MMX, MMX Ext, cmov, 3DNow, 3DNow Ext
    AmdK7Sse        = (CpuVendorAmd + 6),      ///< MMX, MMX Ext, cmov, 3DNow, 3DNow Ext, SSE
    AmdK8           = (CpuVendorAmd + 7),      ///< Athlon 64, Athlon 64 FX, and Opteron
    AmdK10          = (CpuVendorAmd + 8),      ///< Barcelona, Phenom, Greyhound
    AmdFamily12h    = (CpuVendorAmd + 9),      ///< Family 12h - Llano
    AmdBobcat       = (CpuVendorAmd + 10),     ///< Bobcat
    AmdFamily15h    = (CpuVendorAmd + 11),     ///< Family 15h - Orochi, Trinity, Komodo, Kaveri, Basilisk
    AmdFamily16h    = (CpuVendorAmd + 12),     ///< Family 16h - Kabini
    AmdRyzen        = (CpuVendorAmd + 13),     ///< Ryzen
    IntelOld        = (CpuVendorIntel + 0),    ///< Inidicate cpu type befor Intel Pentium III
    IntelP3         = (CpuVendorIntel + 1),    ///< Generic Pentium III
    IntelP3Model7   = (CpuVendorIntel + 2),    ///< PIII-7, PIII Xeon-7
    IntelP3Model8   = (CpuVendorIntel + 3),    ///< PIII-8, PIII Xeon-8, Celeron-8
    IntelPMModel9   = (CpuVendorIntel + 4),    ///< Pentium M Model 9 (Banias)
    IntelXeonModelA = (CpuVendorIntel + 5),    ///< Xeon-A
    IntelP3ModelB   = (CpuVendorIntel + 6),    ///< PIII-B
    IntelPMModelD   = (CpuVendorIntel + 7),    ///< Pentium M Model D (Dothan)
    IntelP4         = (CpuVendorIntel + 8),    ///< Pentium 4, Pentium 4-M, Xenon, Celeron
    IntelPMModelE   = (CpuVendorIntel + 9),    ///< Pentium M Model E (Yonah)
    IntelCoreModelF = (CpuVendorIntel + 10),   ///< Core F (Conroe)
};

/// Specifies a struct that contains information about the system.
struct SystemInfo
{
    CpuType cpuType;                ///< Cpu type
    char    cpuVendorString[16];    ///< Null-terminated cpu vendor string
    char    cpuBrandString[48];     ///< Null-terminated cpu brand string
    uint32  cpuLogicalCoreCount;    ///< Number of logical cores on the cpu
    uint32  cpuPhysicalCoreCount;   ///< Number of physical cores on the cpu
    uint32  totalSysMemSize;        ///< Total system memory (RAM) size in megabytes
    union
    {
        struct
        {
            uint32 affinityMask[RyzenMaxCcxCount]; ///< Affinity mask for each core complex (CCX).
        } amdRyzen; ///< Properties specific to AMD Ryzen CPU's.
    } cpuArchInfo;  ///< This member should be used only for Ryzen for now.
};

/// Queries system information.
///
/// @param [out] pSystemInfo SystemInfo struct containing information about the system.
///
/// @returns Success if querying the system info was successful. Otherwise, the following results will be returned:
///          + ErrorInvalidPointer returned if pSystemInfo is nullptr.
///          + ErrorOutOfMemory returned if the system ran out of memory during the function call.
///          + ErrorUnavailable returned if querying the system info is not supported.
///          + ErrorUnknown returned if an error occurs while calling OS functions.
extern Result QuerySystemInfo(SystemInfo* pSystemInfo);

/// Query cpu type for AMD processor.
///
/// @param [out] pSystemInfo SystemInfo struct containing information about the system.
///
/// @returns none.
extern void QueryAMDCpuType(SystemInfo* pSystemInfo);

/// Query cpu type for Intel processor.
///
/// @param [out] pSystemInfo SystemInfo struct containing information about the system.
///
/// @returns none.
extern void QueryIntelCpuType(SystemInfo* pSystemInfo);

/// Gets the frequency of performance-related queries.
///
/// @returns Current CPU performance counter frequency in Hz.
extern int64 GetPerfFrequency();

/// Gets the current time of a performance-related query.
///
/// This is a high resolution time stamp that can be used in conjunction with GetPerfFrequency to measure time
/// intervals.
///
/// @returns Current value of the CPU performance counter.
extern int64 GetPerfCpuTime();

/// Determines if a specific key is pressed down.
///
/// @param [in]      key        Specified which key to check.
/// @param [in, out] pPrevState The previous state of the key.
///
/// @returns True if the specified key is currently pressed down.
extern bool IsKeyPressed(KeyCode key, bool* pPrevState = nullptr);

/// Retrieves the fully resolved file name of the application binary.
///
/// @param [out] pBuffer      Character buffer to contain the application's executable and (fully-resolved) path
///                           string.
/// @param [out] ppFilename   Pointer to the location within the output buffer where the executable name begins.
/// @param [in]  bufferLength Length of the output buffer, in bytes.
/// @returns Result::Success if GetModuleFileNameA succeeds. Otherwise, the following result codes would be returned:
///          + Result::ErrorInvalidMemorySize returned if pBuffer is not sufficiently large.
extern Result GetExecutableName(
    char*  pBuffer,
    char** ppFilename,
    size_t bufferLength);

/// Retrieves the fully resolved wchar_t file name of the application binary.
///
/// @param [out] pWcBuffer    wchar_t buffer to contain the application's executable and (fully-resolved) path
///                           string.
/// @param [out] ppWcFilename Pointer to the location within the wchar_t output buffer where the executable name begins.
/// @param [in]  bufferLength Length of the output buffer, in bytes.
/// @returns Result::Success if GetModuleFileNameW succeeds. Otherwise, the following result codes would be returned:
///          + Result::ErrorInvalidMemorySize returned if pBuffer is not sufficiently large.
extern Result GetExecutableName(
    wchar_t*  pWcBuffer,
    wchar_t** ppWcFilename,
    size_t    bufferLength);

/// Splits a filename into its path and file components.
///
/// @param [in]  pFullPath  Buffer containing the full path & file name.
/// @param [out] pPathBuf   Optional.  If non-null, will contain the path to the file name.  On Windows, this will also
///                         include the drive letter.
/// @param [in]  pathLen    Length of the pPathBuf buffer.  Must be zero when pPathBuf is null.
/// @param [out] pFileBuf   Optional.  If non-null, will contain the base file name, and extension.
/// @param [in]  fileLen    Length of the pFileBuf buffer.  Must be zero when pFileBuf is null.
extern void SplitFilePath(
    const char* pFullPath,
    char*       pPathBuf,
    size_t      pathLen,
    char*       pFileBuf,
    size_t      fileLen);

/// Creates a new directory at the specified path.
///
/// @param [in] pPathName String specifying the new path to create.  Note that this method can only create one
///                       directory, if you specify "foo/bar" the "bar" directory can only be created if "foo" already
///                       exists.
/// @returns Result::Success if the directory was successfully created, otherwise an appropriate error.  Otherwise, the
///          following result codes may be returned:
///          + Result::AlreadyExists if the specified directory already exists.
///          + Result::ErrorInvalidValue if the parent directory does not exist.
extern Result MkDir(
    const char* pPathName);

/// Creates a new directory at the specified path and all intermediate directories.
///
/// @param [in] pPathName String specifying the new path to create.n
///
/// @returns Result::Success if the directory was successfully created, otherwise an appropriate error.  Otherwise, the
///          following result codes may be returned:
///          + Result::AlreadyExists if the specified directory already exists.
///          + Result::ErrorInvalidValue if the parent directory does not exist.
extern Result MkDirRecursively(
    const char* pPathName);

/// Get the Process ID of the current process
///
/// @returns The Process ID of the current process
extern uint32 GetIdOfCurrentProcess();

/// Flushes CPU cached writes to memory.
PAL_INLINE void FlushCpuWrites()
{
     asm volatile("" ::: "memory");
}

// =====================================================================================================================
// Issue a full memory barrier.
PAL_INLINE void MemoryBarrier()
{
    atomic_thread_fence(std::memory_order_acq_rel);
}

// =====================================================================================================================
/// Issue the cpuid instruction.
///
/// @param [out]  pRegValues  EAX/EBX/ECX/EDX values
/// @param [in]   level       CpuId instruction feature level.
PAL_INLINE void CpuId(
    uint32* pRegValues,
    uint32 level)
{
    __get_cpuid(level, pRegValues, pRegValues + 1, pRegValues + 2, pRegValues + 3);
}

// =====================================================================================================================
/// Issue the cpuid instruction, with an additional sublevel code.
///
/// @param [out]  pRegValues  EAX/EBX/ECX/EDX values
/// @param [in]   level       CpuId instruction feature level.
/// @param [in]   sublevel    CpuId instruction feature sublevel.
PAL_INLINE void CpuId(
    uint32* pRegValues,
    uint32 level,
    uint32 sublevel)
{
    __cpuid_count(level, sublevel, *pRegValues, *(pRegValues + 1), *(pRegValues + 2), *(pRegValues + 3));
}

} // Util

