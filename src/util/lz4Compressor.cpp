/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "lz4hc.h"

namespace Util
{

// =====================================================================================================================
Lz4Compressor::Lz4Compressor(
    const AllocCallbacks& callbacks,
    bool useHighCompression)
    : m_allocator(callbacks)
    , m_useHighCompression(useHighCompression)
    , m_stateInitialized(false)
    , m_pState(nullptr)
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
Lz4Compressor::~Lz4Compressor()
{
    if (m_pState != nullptr)
    {
        PAL_SAFE_FREE(m_pState, &m_allocator);
    }
}

// =====================================================================================================================
Result Lz4Compressor::Init()
{
    Result result = Result::Success;

    if (m_pState == nullptr)
    {
        int stateSize = (m_useHighCompression == true) ? LZ4_sizeofStateHC() : LZ4_sizeofState();
        constexpr int lz4StateAlignmentRequirement = 8;

        m_pState = PAL_MALLOC_ALIGNED(stateSize, lz4StateAlignmentRequirement, &m_allocator, AllocInternal);

        if (m_pState == nullptr)
        {
            result = Result::ErrorOutOfMemory;
        }
    }

    return result;
}

// =====================================================================================================================
int Lz4Compressor::GetCompressBound(
    int inputSize
    ) const
{
    int bound = LZ4_compressBound(inputSize);
    if (bound > 0)
    {
        bound += static_cast<int>(sizeof(FrameHeader));
    }
    return bound;
}

// =====================================================================================================================
int Lz4Compressor::GetDecompressedSize(
    const char* src,
    int srcSize
    ) const
{
    int size = 0;
    if ((srcSize > 0) && (srcSize > static_cast<int>(sizeof(FrameHeader))))
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
    int srcSize,
    int dstCapacity,
    int* pBytesWritten)
{
    Result result = Result::Success;

    // Sanity check/error handling.
    if ((dstCapacity > 0) && (dstCapacity > static_cast<int>(sizeof(FrameHeader))))
    {
        FrameHeader* header = reinterpret_cast<FrameHeader*>(dst);
        header->identifier = HeaderIdentifier;
        header->uncompressedSize = srcSize;
    }
    else
    {
        result = Result::ErrorInvalidMemorySize;
    }

    // Compression
    if (result == Result::Success)
    {
        // Move past the header to the actual lz4 block.
        dstCapacity -= sizeof(FrameHeader);
        dst += sizeof(FrameHeader);

        int returnCode = 0;

        if (m_stateInitialized == false)
        {
            m_stateInitialized = true;

            if (m_useHighCompression == true)
            {
                returnCode =
                    LZ4_compress_HC_extStateHC(m_pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
            else
            {
                returnCode =
                    LZ4_compress_fast_extState(m_pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
        }
        else
        {
            if (m_useHighCompression == true)
            {
                returnCode =
                    LZ4_compress_HC_extStateHC_fastReset(m_pState, src, dst, srcSize, dstCapacity, m_compressionParam);
            }
            else
            {
                returnCode =
                    LZ4_compress_fast_extState_fastReset(m_pState, src, dst, srcSize, dstCapacity, m_compressionParam);
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
    int srcSize,
    int dstCapacity,
    int* pBytesWritten
    ) const
{
    Result result = Result::Success;

    int headerStoredUncompressedSize = 0;

    // Sanity check/error handling.
    if ((srcSize > 0) && (srcSize > static_cast<int>(sizeof(FrameHeader))))
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

} //namespace Util
