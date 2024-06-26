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
/// @return An integer representing the error code of the platform on which the allocation takes place. For example,
/// this call returns EVAL (on Posix) or ERROR_INVALID_PARAMETER (on Windows) when 1) \param requestedBufferSize is 0,
/// or 2) \param pOutBuffer is nullptr. This call returns ERANGE (on Posix) or ERROR_FILE_TOO_LARGE (on Windows), when
/// the actual allocated size is bigger than \ref MirroredBufferMaxSize.
int32_t CreateMirroredBuffer(uint32_t requestedBufferSize, MirroredBuffer* pOutBuffer);

/// Destroy a \ref MirroredBuffer object.
///
/// @param[in/out] pBuffer Pointer to a \ref MirroredBuffer object to be destroyed. The object will be zero'd out.
void DestroyMirroredBuffer(MirroredBuffer* pBuffer);

} // namespace DevDriver
