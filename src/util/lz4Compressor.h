/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palSysMemory.h"
#include "palUtil.h"

namespace Util
{

// =====================================================================================================================
// A class to wrap functionality of the lz4 library for ease of use.
// Note High Compression mode does not appreciably affect decompression time.
// Int types are used for the sizes because that's what the lz4 library uses.
class Lz4Compressor
{
public:
    Lz4Compressor(const AllocCallbacks& callbacks, bool useHighCompression = false);
    virtual ~Lz4Compressor();

    Result Init();

    // Provides the maximum size that LZ4 compression may output in a "worst case" (uncompressible) scenario.
    // Use this to determine the size of the buffer to send to Compress().
    // Returns 0 on error.
    int GetCompressBound(int inputSize) const;

    // Provides the decompressed size from a compressed buffer.
    // Returns 0 on error.
    int GetDecompressedSize(const char* src, int srcSize) const;

    // Helper for easy readability.
    bool IsCompressed(const char* src, int srcSize) const { return (GetDecompressedSize(src, srcSize) > 0); }

    // Compress relies on state, and should be protected via a mutex externally. This function is not thread-safe.
    // The destination buffer should be allocated ahead of time and be of GetCompressBound() size.
    Result Compress(const char* src, char* dst, int srcSize, int dstCapacity, int* pBytesWritten);

    // Note that decompress doesn't rely on state, and so can be called from multiple threads.
    // The destination buffer should be allocated ahead of time and be of GetCompressBound() size.
    Result Decompress(const char* src, char* dst, int srcSize, int dstCapacity, int* pBytesWritten) const;

    // If useHighCompression is false, this corresponds with the lz4 "acceleration" param.
    // The larger the param, the faster (and less compression) you get.
    // If useHighCompression is true, this corresponds with the lz4hc "compressionLevel" param.
    // In this case, the larger the param the slower (and more compression) we get.
    // ...
    // We choose sane values by default, so this is only for fine tuning.
    void SetCompressionParam(int param) { m_compressionParam = param; }

private:
    PAL_DISALLOW_DEFAULT_CTOR(Lz4Compressor);
    PAL_DISALLOW_COPY_AND_ASSIGN(Lz4Compressor);

    struct FrameHeader
    {
        int32 identifier;
        int32 uncompressedSize;
    };

    ForwardAllocator m_allocator;
    const bool m_useHighCompression;
    int   m_compressionParam;
    bool  m_stateInitialized;
    void* m_pState;

    static const int32 HeaderIdentifier = 0x504c5a34; // 'PLZ4' in a portable constant.
};

} //namespace Util
