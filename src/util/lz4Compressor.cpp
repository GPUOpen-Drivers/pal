/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "lz4Compressor.h"

#include "palInlineFuncs.h"
#include "lz4hc.h"

namespace Util
{
// Used to store state information on a thread by thread basis.
// We need to create our own global allocator because it needs to last the lifetime of the process,
// since we do not know when threads will be destroyed.
GenericAllocator g_lz4CompressorGlobalAllocator{};
thread_local Lz4Compressor::ThreadLocalData g_lz4CompressorThreadLocalData;

// =====================================================================================================================
Lz4Compressor::Lz4Compressor(
    bool useHighCompression)
    : m_useHighCompression(useHighCompression)
{
    if (m_useHighCompression == true)
    {
        m_compressionParam = LZ4HC_CLEVEL_DEFAULT;
    }
    else
    {
        m_compressionParam = 1;
    }
}

// =====================================================================================================================
int32 Lz4Compressor::GetCompressBound(
    int32 inputSize
    ) const
{
    int32 bound = LZ4_compressBound(inputSize);
    if (bound > 0)
    {
        bound += static_cast<int32>(sizeof(FrameHeader));
    }

    return bound;
}

// =====================================================================================================================
int32 Lz4Compressor::GetDecompressedSize(
    const char* src,
    int32 srcSize
    ) const
{
    int32 size = 0;
    if ((srcSize > 0) && (srcSize > static_cast<int32>(sizeof(FrameHeader))))
    {
        const FrameHeader* header = reinterpret_cast<const FrameHeader*>(src);
        if (header->identifier == HeaderIdentifier)
        {
            size = header->uncompressedSize;
        }
    }

    return size;
}

// =====================================================================================================================
Result Lz4Compressor::Compress(
    const char* src,
    char* dst,
    int32 srcSize,
    int32 dstCapacity,
    int32* pBytesWritten)
{
    Result result = Result::Success;

    // Allocate (thread local) state if necessary.
    bool stateNeedsInit = false;
    void* pState = nullptr;
    if (result == Result::Success)
    {
        pState = (m_useHighCompression == false) ? g_lz4CompressorThreadLocalData.m_pState :
            g_lz4CompressorThreadLocalData.m_pStateHC;

        if (pState == nullptr)
        {
            stateNeedsInit = true;

            constexpr int32 Lz4StateAlignmentRequirement = 8;
            // We want to align to cache line size to ensure no sharing of cache lines between cores.
            constexpr int32 stateAlignment = Util::Max(Lz4StateAlignmentRequirement, PAL_CACHE_LINE_BYTES);

            // NOTE:
            // We use global malloc for thread local data as it persists for the lifetime of the thread,
            // and we otherwise have no guarantee any other allocator we may use is still around when the thread ends.
            pState = PAL_MALLOC_ALIGNED(((m_useHighCompression == false) ? LZ4_sizeofState() : LZ4_sizeofStateHC()),
                stateAlignment,
                &g_lz4CompressorGlobalAllocator,
                AllocInternal);

            if (pState == nullptr)
            {
                result = Result::ErrorOutOfMemory;
            }
            else
            {
                // Store in the thread local data
                if (m_useHighCompression == false)
                {
                    g_lz4CompressorThreadLocalData.m_pState = pState;
                }
                else
                {
                    g_lz4CompressorThreadLocalData.m_pStateHC = pState;
                }
            }
        }
    }

    // Sanity check/error handling.
    if (result == Result::Success)
    {
        if ((dstCapacity > 0) && (dstCapacity > static_cast<int32>(sizeof(FrameHeader))))
        {
            FrameHeader* header = reinterpret_cast<FrameHeader*>(dst);
            header->identifier = HeaderIdentifier;
            header->uncompressedSize = srcSize;
        }
        else
        {
            result = Result::ErrorInvalidMemorySize;
        }
    }

    // Compression
    if (result == Result::Success)
    {
        // Move past the header to the actual lz4 block.
        dstCapacity -= sizeof(FrameHeader);
        dst += sizeof(FrameHeader);

        int returnCode = 0;

        if (stateNeedsInit == true)
        {
            if (m_useHighCompression == true)
            {
                returnCode =
                    LZ4_compress_HC_extStateHC(pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
            else
            {
                returnCode =
                    LZ4_compress_fast_extState(pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
        }
        else
        {
            if (m_useHighCompression == true)
            {
                returnCode =
                    LZ4_compress_HC_extStateHC_fastReset(pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
            else
            {
                returnCode =
                    LZ4_compress_fast_extState_fastReset(pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
        }

        if (pBytesWritten != nullptr)
        {
            if (returnCode < 0)
            {
                result = Result::ErrorUnknown;
                *pBytesWritten = 0;
            }
            else
            {
                *pBytesWritten = sizeof(FrameHeader) + returnCode;
            }
        }
    }

    return result;
}

// =====================================================================================================================
Result Lz4Compressor::Decompress(
    const char* src,
    char* dst,
    int32 srcSize,
    int32 dstCapacity,
    int32* pBytesWritten
    ) const
{
    Result result = Result::Success;

    int32 headerStoredUncompressedSize = 0;

    // Sanity check/error handling.
    if ((srcSize > 0) && (srcSize > static_cast<int32>(sizeof(FrameHeader))))
    {
        const FrameHeader* header = reinterpret_cast<const FrameHeader*>(src);
        if (header->identifier == HeaderIdentifier)
        {
            headerStoredUncompressedSize = header->uncompressedSize;
            if (header->uncompressedSize > dstCapacity)
            {
                result = Result::ErrorInvalidMemorySize;
            }
        }
        else
        {
            result = Result::ErrorInvalidFormat;
        }
    }
    else
    {
        result = Result::ErrorInvalidMemorySize;
    }

    // Decompression
    if (result == Result::Success)
    {
        //Move past the header to the actual lz4 block.
        srcSize -= sizeof(FrameHeader);
        src += sizeof(FrameHeader);

        int returnCode = LZ4_decompress_safe(src, dst, srcSize, dstCapacity);

        PAL_ASSERT(headerStoredUncompressedSize == returnCode);

        if (pBytesWritten != nullptr)
        {
            if (returnCode < 0)
            {
                result = Result::ErrorUnknown;
                *pBytesWritten = 0;
            }
            else
            {
                *pBytesWritten = returnCode;
            }
        }
    }

    return result;
}

// =====================================================================================================================
Lz4Compressor::ThreadLocalData::~ThreadLocalData()
{
    PAL_SAFE_FREE(m_pState, &g_lz4CompressorGlobalAllocator);
    PAL_SAFE_FREE(m_pStateHC, &g_lz4CompressorGlobalAllocator);
}

} //namespace Util
