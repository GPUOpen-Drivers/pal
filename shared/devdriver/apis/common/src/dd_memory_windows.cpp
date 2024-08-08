/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include <dd_assert.h>
#include <dd_integer.h>
#include <dd_memory.h>
#include <Windows.h>

#if !defined(DD_TARGET_PLATFORM_WINDOWS)
#error "This file must be compiled for Windows platform"
#endif

namespace DevDriver
{

int32_t CreateMirroredBuffer(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer)
{
    if ((requestedBufferSize == 0) || (pOutBuffer == nullptr))
    {
        return ERROR_INVALID_PARAMETER;
    }

    SYSTEM_INFO sysInfo {};
    GetSystemInfo(&sysInfo);
    const uint32_t pageSize = sysInfo.dwPageSize;

    const uint32_t pageSizeAlignedBufferSize = AlignU32(requestedBufferSize, pageSize);
    const uint32_t actualBufferSize = NextSmallestPow2(pageSizeAlignedBufferSize);

    if ((actualBufferSize == 0) || (actualBufferSize > MirroredBufferMaxSize))
    {
        return ERROR_FILE_TOO_LARGE;
    }

    int32_t err = 0;

    const uint32_t virtualMemorySize = actualBufferSize * 2;
    DD_ALWAYS_ASSERT(virtualMemorySize > actualBufferSize); // check overflow

    void* pPlaceholder1 = nullptr;
    void* pPlaceholder2 = nullptr;

    if (err == 0)
    {
        pPlaceholder1 = VirtualAlloc2(
            NULL, // Process
            NULL, // BaseAddress
            virtualMemorySize, // Size
            MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, // AllocationType
            PAGE_NOACCESS, // PageProtection
            NULL, // ExtendedParameters
            0     // ParameterCount
        );

        if (pPlaceholder1 == nullptr)
        {
            err = GetLastError();
        }
    }

    if (err == 0)
    {
        // Split one placeholder into two.
        BOOL success = VirtualFree(pPlaceholder1, actualBufferSize, MEM_PRESERVE_PLACEHOLDER | MEM_RELEASE);
        if (success)
        {
            pPlaceholder2 = reinterpret_cast<void*>((ULONG_PTR)pPlaceholder1 + actualBufferSize);
        }
        else
        {
            err = GetLastError();
        }
    }

    HANDLE hPhysicalMemory = INVALID_HANDLE_VALUE;

    if (err == 0)
    {
        hPhysicalMemory = CreateFileMapping(
            INVALID_HANDLE_VALUE, // hFile, use system memory instead of file on disk
            NULL,                 // lpFileMappingAttributes
            PAGE_READWRITE,       // page protection
            0,                    // high-order dword of the maximum size
            actualBufferSize,     // low-order dword of the maximum size
            NULL                  // name of the file mapping object
        );

        if (hPhysicalMemory == NULL)
        {
            err = GetLastError();
        }
    }

    void* pBuffer = nullptr;
    void* pWrapBuffer = nullptr;

    if (err == 0)
    {
        // Map the first half of virtual memory to the physical memory.
        pBuffer = MapViewOfFile3(
            hPhysicalMemory,         // file-mapping object
            NULL,                    // process
            pPlaceholder1,           // base address
            0,                       // offset
            actualBufferSize,        // size
            MEM_REPLACE_PLACEHOLDER, // allocation type
            PAGE_READWRITE,          // page protection
            nullptr,                 // ExtendedParameters
            0                        // parameter count
        );

        if (pBuffer == nullptr)
        {
            err = GetLastError();
        }
    }

    if (err == 0)
    {
        // Map the second half of virtual memory to the same physical memory.
        pWrapBuffer = MapViewOfFile3(
            hPhysicalMemory,         // file-mapping object
            NULL,                    // process
            pPlaceholder2,           // base address
            0,                       // offset
            actualBufferSize,        // size
            MEM_REPLACE_PLACEHOLDER, // allocation type
            PAGE_READWRITE,          // page protection
            nullptr,                 // ExtendedParameters
            0                        // parameter count
        );

        if (pWrapBuffer == nullptr)
        {
            err = GetLastError();
        }
    }

    if (err != 0)
    {
        if (pWrapBuffer != nullptr)
        {
            UnmapViewOfFile(pWrapBuffer);
        }
        if (pBuffer != nullptr)
        {
            UnmapViewOfFile(pBuffer);
        }
        if (hPhysicalMemory != NULL)
        {
            CloseHandle(hPhysicalMemory);
        }
        if (pPlaceholder2 != nullptr)
        {
            VirtualFree(pPlaceholder2, 0, MEM_RELEASE);
        }
        if (pPlaceholder1 != nullptr)
        {
            VirtualFree(pPlaceholder1, 0, MEM_RELEASE);
        }

        pOutBuffer->bufferSize  = 0;
        pOutBuffer->pBuffer     = nullptr;
    }
    else
    {
        pOutBuffer->bufferSize  = actualBufferSize;
        pOutBuffer->pBuffer     = pBuffer;

        // Mappings create above hold references to this handle, so it can be safely closed.
        CloseHandle(hPhysicalMemory);
    }

    return err;
}

void DestroyMirroredBuffer(MirroredBuffer* pBuffer)
{
    void* pSecondaryBuffer = (uint8_t*)pBuffer->pBuffer + pBuffer->bufferSize;

    BOOL success = UnmapViewOfFile(pSecondaryBuffer);
    DD_ASSERT(success == TRUE);

    success = UnmapViewOfFile(pBuffer->pBuffer);
    DD_ASSERT(success == TRUE);
    pBuffer->pBuffer = nullptr;

    pBuffer->bufferSize = 0;
}

} // namespace DevDriver
