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

#include <dd_memory.h>
#include <dd_integer.h>

namespace DevDriver
{

DD_RESULT ScratchBuffer::Initialize(uint32_t totalSize, uint32_t initialCommitSize)
{
    if ((totalSize == 0) || (totalSize < initialCommitSize))
    {
        return DD_RESULT_COMMON_INVALID_PARAMETER;
    }

    m_pageSize = GetPageSize();

    const uint32_t pageSizeAlignedTotalSize = AlignU32(totalSize, m_pageSize);
    const uint32_t pageSizeAlignedInitialCommitSize = AlignU32(initialCommitSize, m_pageSize);

    // Reserve virtual memory.
    DD_RESULT result = ReserveMemory(pageSizeAlignedTotalSize, reinterpret_cast<void**>(&m_pBuffer));
    if (result == DD_RESULT_SUCCESS)
    {
        m_totalSize = pageSizeAlignedTotalSize;
    }

    // Commit a part of virtual memory to physical memory.
    if (result == DD_RESULT_SUCCESS)
    {
        result = CommitMemory(pageSizeAlignedInitialCommitSize);
        if (result == DD_RESULT_SUCCESS)
        {
            m_committedSize = pageSizeAlignedInitialCommitSize;
        }
    }

    if (result != DD_RESULT_SUCCESS)
    {
        m_pBuffer = nullptr;
    }

    return result;
}

void ScratchBuffer::Destroy()
{
    FreeMemory(m_pBuffer, m_totalSize);
}

void* ScratchBuffer::Push(uint32_t size)
{
    if (size > (m_totalSize - m_top))
    {
        return nullptr;
    }

    DD_RESULT result = DD_RESULT_SUCCESS;
    void* pResultMem = nullptr;

    uint32_t alignedSizeToCommit = 0;
    if (size > (m_committedSize - m_top))
    {
        const uint32_t extraSizeNeeded = size - (m_committedSize - m_top);
        alignedSizeToCommit = AlignU32(extraSizeNeeded, m_pageSize);
        result = CommitMemory(alignedSizeToCommit);
    }

    if (result == DD_RESULT_SUCCESS)
    {
        m_committedSize += alignedSizeToCommit;
        pResultMem = m_pBuffer + m_top;
        m_top += size;
    }

    return pResultMem;
}

void ScratchBuffer::Pop(uint32_t size)
{
    DD_ASSERT(size <= m_top);
    m_top -= size;
}

void ScratchBuffer::Clear()
{
    m_top = 0;
}

} // namespace DevDriver
