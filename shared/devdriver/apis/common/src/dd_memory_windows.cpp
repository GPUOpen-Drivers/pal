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
#include <dd_result.h>
#include <Windows.h>

#if !defined(DD_TARGET_PLATFORM_WINDOWS)
#error "This file must be compiled for Windows platform"
#endif

namespace DevDriver
{

DD_RESULT MirroredBufferCreate(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer)
{
    if ((requestedBufferSize == 0) || (pOutBuffer == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    SYSTEM_INFO sysInfo {};
    GetSystemInfo(&sysInfo);
    const uint32_t pageSize = sysInfo.dwPageSize;

    const uint32_t pageSizeAlignedBufferSize = AlignU32(requestedBufferSize, pageSize);
    const uint32_t actualBufferSize = NextSmallestPow2(pageSizeAlignedBufferSize);

    if ((actualBufferSize == 0) || (actualBufferSize > MirroredBufferMaxSize))
    {
        return DD_RESULT_COMMON_OUT_OF_RANGE;
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

    return ResultFromWin32Error(err);
}

void MirroredBufferDestroy(MirroredBuffer* pBuffer)
{
    void* pSecondaryBuffer = (uint8_t*)pBuffer->pBuffer + pBuffer->bufferSize;

    BOOL success = UnmapViewOfFile(pSecondaryBuffer);
    DD_ASSERT(success == TRUE);

    success = UnmapViewOfFile(pBuffer->pBuffer);
    DD_ASSERT(success == TRUE);
    pBuffer->pBuffer = nullptr;

    pBuffer->bufferSize = 0;
}

uint32_t ScratchBuffer::GetPageSize()
{
    SYSTEM_INFO sysInfo {};
    GetSystemInfo(&sysInfo);
    return sysInfo.dwPageSize;
}

DD_RESULT ScratchBuffer::ReserveMemory(uint32_t size, void** ppOutBuffer)
{
    DD_ASSERT((size & (m_pageSize - 1)) == 0);

    DD_RESULT result = DD_RESULT_SUCCESS;
    void* pMemory = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
    if (pMemory)
    {
        *ppOutBuffer = pMemory;
    }
    else
    {
        result = ResultFromWin32Error(GetLastError());
    }
    return result;
}

void ScratchBuffer::FreeMemory(void* pMemory, uint32_t size)
{
    // Pass 0 to free the entire virtual memory
    size = 0;
    BOOL success = VirtualFree(pMemory, size, MEM_RELEASE);
    DD_ASSERT(success == TRUE);
}

DD_RESULT ScratchBuffer::CommitMemory(uint32_t size)
{
    DD_ASSERT((size & (m_pageSize - 1)) == 0);

    DD_RESULT result = DD_RESULT_SUCCESS;
    if (size > 0)
    {
        LPVOID pBaseMem = VirtualAlloc(m_pBuffer + m_committedSize, size, MEM_COMMIT, PAGE_READWRITE);
        if (pBaseMem == nullptr)
        {
            result = ResultFromWin32Error(GetLastError());
        }
    }
    return result;
}

} // namespace DevDriver
