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

#pragma once

#include <dd_common_api.h>

namespace DevDriver
{

/// The maximum size of physical memory that can be allocated.
static constexpr uint32_t MirroredBufferMaxSize = 1 << 30; // 1GB

/// MirroredBuffer contains a block of continous virtual memory divided into two buffers of equal size, both of which
/// are mapped to the same physical memory. It is primarily used to make handling wrap-around in circular buffer easier.
struct MirroredBuffer
{
    /// The primary buffer used for reading/writing data.
    void*    pBuffer;

    // The size of the backing storage actually allocated.
    uint32_t bufferSize;
};

/// Create a MirroredBuffer.
///
/// @param[in] requestedBufferSize Hint for how big of a buffer to allocate. The actual size of the allocated buffer is
/// aligned at page-size boundary, and adjusted to be power of 2. So the actual size can be bigger than the requested
/// size.
/// @param[out] pOutBuffer Pointer to a \ref MirroredBuffer object to be initialized if allocation succeeds.
/// @return DD_RESULT_COMMON_INVALID_PARAMETER if 1) \param requestedBufferSize is 0, or 2) \param pOutBuffer is nullptr.
/// @return DD_RESULT_COMMON_OUT_OF_RANGE if the actual to-be-allocated size is bigger than \ref MirroredBufferMaxSize.
/// @return other errors
DD_RESULT MirroredBufferCreate(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer);

/// Destroy a \ref MirroredBuffer object.
///
/// @param[in/out] pBuffer Pointer to a \ref MirroredBuffer object to be destroyed. The object will be zero'd out.
void MirroredBufferDestroy(MirroredBuffer* pBuffer);

/// A scratch buffer is a region of memory used for allocating objects with short lifetime. Memory from the scratch
/// buffer is allocated in a linear, stack-based fashion. Because memory pages in the scratch buffer is committed to
/// physical memory as they're accessed, scratch buffers can be initialized to very large size (e.g. 1GB) without
/// consuming much physical memory.
class ScratchBuffer
{
private:
    uint32_t m_totalSize;
    uint32_t m_committedSize;
    uint32_t m_pageSize;

    uint32_t m_top;
    uint8_t* m_pBuffer;

public:
    ScratchBuffer()
        : m_totalSize {0}
        , m_committedSize {0}
        , m_pageSize {0}
        , m_top {0}
        , m_pBuffer {nullptr}
    {}

    /// Initialze the scratch buffer.
    ///
    /// @param[in] totalSize Total amount of memory the scratch buffer can hold.
    /// @param[in] initialCommittedSize Initial amount of physical memory committed. This parameter is ignored on Linux.
    /// @return DD_RESULT_SUCCESS if scratch buffer is initialized successfully.
    /// @return other error codes if initialization failed.
    DD_RESULT Initialize(uint32_t totalSize, uint32_t initialCommittedSize);

    /// Destroy the scratch buffer. Accessing the scratch buffer after its destruction is undefined behavior.
    void Destroy();

    /// Allocate a block of memory from the stack.
    ///
    /// @param[in] size The amount of memory to allocate.
    /// @return A pointer to the beginning of the allocated memory.
    /// @return nullptr if size is bigger than the remaining free memory in the scratch buffer.
    void* Push(uint32_t size);

    /// Free a block of memory from the stack, essentially moves back the top of the stack by the specified amount.
    /// Note, currently scratch buffers don't de-commit unused physical memory.
    ///
    /// @param[in] size The amount of memory to free.
    void Pop(uint32_t size);

    /// De-allocated memory from the stack, essentially reset the top of the stack to zero.
    void Clear();

private:
    DD_RESULT ReserveMemory(uint32_t size, void** ppOutMemory);
    void      FreeMemory(void* pMemory, uint32_t size);
    DD_RESULT CommitMemory(uint32_t size);
};

} // namespace DevDriver
