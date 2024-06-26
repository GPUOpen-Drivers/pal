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

#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

namespace DevDriver
{

int32_t CreateMirroredBuffer(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer)
{
    if ((requestedBufferSize == 0) || (pOutBuffer == nullptr))
    {
        return EINVAL;
    }

    const uint32_t pageSize = getpagesize();
    DD_ASSERT((pageSize & (pageSize - 1)) == 0); // pageSize must be power of 2.

    const uint32_t alignedBufferSize = (requestedBufferSize + (pageSize - 1)) & (~(pageSize - 1));
    const uint32_t actualBufferSize = NextSmallestPow2(alignedBufferSize);

    if ((actualBufferSize == 0) || (actualBufferSize > MirroredBufferMaxSize))
    {
        return ERANGE;
    }

    int err = 0;

    const uint32_t virtualMemorySize = actualBufferSize * 2;
    DD_ASSERT(virtualMemorySize > actualBufferSize); // check overflow

    int physicalMemoryFd = memfd_create("physical_buffer", 0);
    if (physicalMemoryFd == -1)
    {
        err = errno;
    }
    else
    {
        if (ftruncate(physicalMemoryFd, actualBufferSize) == -1)
        {
            err = errno;
        }
    }

    void* pWholeBuffer = nullptr;
    void* pPrimaryBuffer = nullptr;
    void* pSecondaryBuffer = nullptr;

    if (err == 0)
    {
        // Allocate a virtual memory block without any backing storage.
        pWholeBuffer = mmap(
            nullptr,                     // Let the kernel choose a base virtual address for us.
            virtualMemorySize,           // size
            PROT_NONE,                   // protection
            MAP_PRIVATE | MAP_ANONYMOUS, // The mapping is only visible in this process, and will be backed by physical
                                         // memory instead of not file.
            -1,                          // No backing storage.
            0                            // offset
        );

        if (pWholeBuffer == nullptr)
        {
            err = errno;
        }
    }

    if (err == 0)
    {
        // Map the first half of the virtual memory to the physical memory.
        pPrimaryBuffer = mmap(
            pWholeBuffer,
            actualBufferSize,
            PROT_READ | PROT_WRITE, // protection
            MAP_FIXED | MAP_SHARED, // Place the mapping at exactly `pWholeBuffer`.
            physicalMemoryFd,       // mapped to the physical memory
            0                       // offset
        );

        if (pPrimaryBuffer == nullptr)
        {
            err = errno;
        }
        else
        {
            pWholeBuffer = nullptr;
        }
    }

    if (err == 0)
    {
        // Map the second half to the physical memory.
        pSecondaryBuffer = mmap(
            reinterpret_cast<uint8_t*>(pPrimaryBuffer) + actualBufferSize, // starting address of the second buffer
            actualBufferSize,       // size
            PROT_READ | PROT_WRITE, // protection
            MAP_FIXED | MAP_SHARED, // Place the mapping at exactly the beginning of the second buffer.
            physicalMemoryFd,       // mapped to the physical memory
            0                       // offset
        );

        if (pSecondaryBuffer == nullptr)
        {
            err = errno;
        }
    }

    if (err != 0)
    {
        if (pSecondaryBuffer != nullptr)
        {
            munmap(pSecondaryBuffer, actualBufferSize);
        }
        if (pPrimaryBuffer != nullptr)
        {
            munmap(pPrimaryBuffer, actualBufferSize);
        }
        if (pWholeBuffer != nullptr)
        {
            munmap(pWholeBuffer, actualBufferSize);
        }
        if (physicalMemoryFd != -1)
        {
            close(physicalMemoryFd);
        }

        pOutBuffer->bufferSize  = 0;
        pOutBuffer->pBuffer     = nullptr;
    }
    else
    {
        pOutBuffer->bufferSize  = actualBufferSize;
        pOutBuffer->pBuffer     = pPrimaryBuffer;

        // Mappings created above hold references to this fd, so it can be safely closed.
        close(physicalMemoryFd);
        physicalMemoryFd = -1;
    }

    return err;
}

void DestroyMirroredBuffer(MirroredBuffer* pBuffer)
{
    void* pSecondaryBuffer = (uint8_t*)pBuffer->pBuffer + pBuffer->bufferSize;

    int err = munmap(pSecondaryBuffer, pBuffer->bufferSize);
    DD_ASSERT(err == 0);

    err = munmap(pBuffer->pBuffer, pBuffer->bufferSize);
    DD_ASSERT(err == 0);
    pBuffer->pBuffer = nullptr;

    pBuffer->bufferSize = 0;
}

} // namespace DevDriver
