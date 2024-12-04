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

#include <stb_sprintf.h>

#include <atomic>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

namespace DevDriver
{

DD_RESULT MirroredBufferCreate(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer)
{
    if ((requestedBufferSize == 0) || (pOutBuffer == nullptr))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    const uint32_t pageSize = getpagesize();

    const uint32_t alignedBufferSize = AlignU32(requestedBufferSize, pageSize);
    const uint32_t actualBufferSize = NextSmallestPow2(alignedBufferSize);

    if ((actualBufferSize == 0) || (actualBufferSize > MirroredBufferMaxSize))
    {
        return DD_RESULT_COMMON_OUT_OF_RANGE;
    }

    int err = 0;

    const uint32_t virtualMemorySize = actualBufferSize * 2;
    DD_ALWAYS_ASSERT(virtualMemorySize > actualBufferSize); // check overflow

    const size_t ShmNameBufSizeMax = 64;
    char shmNameBuf[ShmNameBufSizeMax] {};

    // Generate a unique name across threads and processes.
    static std::atomic_uint32_t s_uniqueCounter = 0;
    pid_t pid = getpid();
    uint32_t counter = s_uniqueCounter.fetch_add(1, std::memory_order_seq_cst);
    int printSize = stbsp_sprintf(shmNameBuf, "/%u_%u_mirrored_buffer_2cafe0f9_916c", pid, counter);
    DD_ASSERT(printSize > 0);

    // Use `O_EXCL` to disallow opening of an existing shared memory object.
    // We can't use `memfd_create()` because it's available in Linux kernel >= 3.17, and this code needs to be
    // run on older Linux kernel.
    int physicalMemoryFd = shm_open(shmNameBuf, O_RDWR | O_CREAT | O_EXCL, 0);
    if (physicalMemoryFd == -1)
    {
        err = errno;
    }
    else
    {
        err = shm_unlink(shmNameBuf);
        if (err == 0)
        {
            if (ftruncate(physicalMemoryFd, actualBufferSize) == -1)
            {
                err = errno;
            }
        }
        else
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
        // Map the second half to the same physical memory.
        pSecondaryBuffer = mmap(
            reinterpret_cast<uint8_t*>(pPrimaryBuffer) + actualBufferSize, // starting address of the second buffer
            actualBufferSize,       // size
            PROT_READ | PROT_WRITE, // protection
            MAP_FIXED | MAP_SHARED, // Place the mapping at exactly the beginning of the second buffer.
            physicalMemoryFd,       // mapped to the same physical memory
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

    return ResultFromErrno(err);
}

void MirroredBufferDestroy(MirroredBuffer* pBuffer)
{
    void* pSecondaryBuffer = (uint8_t*)pBuffer->pBuffer + pBuffer->bufferSize;

    int err = munmap(pSecondaryBuffer, pBuffer->bufferSize);
    DD_ASSERT(err == 0);

    err = munmap(pBuffer->pBuffer, pBuffer->bufferSize);
    DD_ASSERT(err == 0);
    pBuffer->pBuffer = nullptr;

    pBuffer->bufferSize = 0;
}

uint32_t ScratchBuffer::GetPageSize()
{
    return getpagesize();;
}

DD_RESULT ScratchBuffer::ReserveMemory(uint32_t size, void** ppOutBuffer)
{
    DD_ASSERT((size & (m_pageSize - 1)) == 0);

    DD_RESULT result = DD_RESULT_SUCCESS;
    void* pMemory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pMemory == MAP_FAILED)
    {
        result = ResultFromErrno(errno);
    }
    else
    {
        *ppOutBuffer = pMemory;
    }
    return result;
}

void ScratchBuffer::FreeMemory(void* pMemory, uint32_t size)
{
    int err = munmap(pMemory, size);
    DD_ASSERT(err == 0);
}

DD_RESULT ScratchBuffer::CommitMemory(uint32_t size)
{
    DD_ASSERT((size & (m_pageSize - 1)) == 0);
    // On linux, there is no way to commit physical memory. Memory pages are committed as they are
    // accessed.
    return DD_RESULT_SUCCESS;
}

} // namespace DevDriver
