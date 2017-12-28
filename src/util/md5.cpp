/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFile.h"
#include "palMd5.h"
#include "palSysMemory.h"

#define F1(x, y, z) (z ^ (x & (y ^ z)))
#define F2(x, y, z) F1(z, x, y)
#define F3(x, y, z) (x ^ y ^ z)
#define F4(x, y, z) (y ^ (x | ~z))

// This is the central step in the MD5 algorithm.
#define MD5STEP(f, w, x, y, z, data, s) \
    ( w += f(x, y, z) + data,  w = w<<s | w>>(32-s),  w += x )

namespace Util
{
namespace Md5
{

static void Transform(uint32* pBuf, uint32* pIn);
static void ByteReverse(uint8* pBuf, uint32 longs);

// =====================================================================================================================
// Generates a checksum on the specified buffer using the MD5 algorithm.
Hash GenerateHashFromBuffer(
    const void*  pBuffer,
    size_t bufLen)
{
    Context ctx;
    Init(&ctx);

    Update(&ctx, static_cast<const uint8*>(pBuffer), bufLen);

    Hash hash;
    Final(&ctx, &hash);

    return hash;
}

// =====================================================================================================================
// Initializes the context for the MD5 algorithm using some magic values.
void Init(
    Context* pCtx)
{
    pCtx->buf[0] = 0x67452301;
    pCtx->buf[1] = 0xefcdab89;
    pCtx->buf[2] = 0x98badcfe;
    pCtx->buf[3] = 0x10325476;

    pCtx->bits[0] = 0;
    pCtx->bits[1] = 0;
}

// =====================================================================================================================
// Updates the context to reflect the concatenation of another buffer full of data for the algorithm.
void Update(
    Context*     pCtx,
    const uint8* pBuf,
    size_t       bufLen)
{
    bool breakVal = false;

    uint32 value = pCtx->bits[0];
    pCtx->bits[0] = value + static_cast<uint32>(bufLen << 3);
    if (pCtx->bits[0] < value)
    {
        pCtx->bits[1]++;
    }
    pCtx->bits[1] += static_cast<uint32>(bufLen >> 29);

    value = (value >> 3) & 0x3f;

    if (value)
    {
        uint8* pWorkingBuffer = static_cast<uint8*>(pCtx->in + value);

        value = 64 - value;

        if (bufLen < value)
        {
            memcpy(pWorkingBuffer, pBuf, bufLen);
            breakVal = true;
        }
        else
        {
            memcpy(pWorkingBuffer, pBuf, value);
            ByteReverse(&pCtx->in[0], 16);
            Transform(&pCtx->buf[0], reinterpret_cast<uint32*>(&pCtx->in[0]));
            pBuf += value;
            bufLen -= value;
        }
    }

    if (breakVal == false)
    {
        while (bufLen >= 64)
        {
            memcpy(&pCtx->in[0], pBuf, 64);
            ByteReverse(&pCtx->in[0], 16);
            Transform(&pCtx->buf[0], reinterpret_cast<uint32*>(&pCtx->in[0]));
            pBuf += 64;
            bufLen -= 64;
        }

        memcpy(&pCtx->in[0], pBuf, bufLen);
    }
}

// =====================================================================================================================
// Finalizes the context and outputs the checksum.
void Final(
    Context* pCtx,
    Hash*    pHash)
{
    uint32 count = (pCtx->bits[0] >> 3) & 0x3F;

    uint8* pBuf = pCtx->in + count;
    *pBuf++ = 0x80;

    count = 64 - 1 - count;

    if (count < 8)
    {
        memset(pBuf, 0, count);
        ByteReverse(&pCtx->in[0], 16);
        Transform(&pCtx->buf[0], reinterpret_cast<uint32*>(&pCtx->in[0]));
        memset(&pCtx->in[0], 0, 56);
    }
    else
    {
        memset(pBuf, 0, count - 8);
    }

    ByteReverse(&pCtx->in[0], 14);

    reinterpret_cast<uint32*>(&pCtx->in[0])[14] = pCtx->bits[0];
    reinterpret_cast<uint32*>(&pCtx->in[0])[15] = pCtx->bits[1];

    Transform(&pCtx->buf[0], reinterpret_cast<uint32*>(&pCtx->in[0]));
    ByteReverse(reinterpret_cast<uint8*>(&pCtx->buf[0]), 4);
    memcpy(pHash->hashValue, &pCtx->buf[0], 16);
}

// =====================================================================================================================
// Performs the actual checksumming on the input data.
void Transform(
    uint32*  pBuf,  // [in, out] Working buffer.
    uint32*  pIn)   // [in] Buffer of the current checksum information.
{
    uint32 a = pBuf[0];
    uint32 b = pBuf[1];
    uint32 c = pBuf[2];
    uint32 d = pBuf[3];

    MD5STEP(F1, a, b, c, d, pIn[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, pIn[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, pIn[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, pIn[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, pIn[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, pIn[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, pIn[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, pIn[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, pIn[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, pIn[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, pIn[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, pIn[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, pIn[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, pIn[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, pIn[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, pIn[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, pIn[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, pIn[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, pIn[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, pIn[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, pIn[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, pIn[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, pIn[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, pIn[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, pIn[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, pIn[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, pIn[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, pIn[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, pIn[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, pIn[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, pIn[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, pIn[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, pIn[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, pIn[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, pIn[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, pIn[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, pIn[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, pIn[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, pIn[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, pIn[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, pIn[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, pIn[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, pIn[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, pIn[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, pIn[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, pIn[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, pIn[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, pIn[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, pIn[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, pIn[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, pIn[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, pIn[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, pIn[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, pIn[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, pIn[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, pIn[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, pIn[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, pIn[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, pIn[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, pIn[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, pIn[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, pIn[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, pIn[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, pIn[9] + 0xeb86d391, 21);

    pBuf[0] += a;
    pBuf[1] += b;
    pBuf[2] += c;
    pBuf[3] += d;
}

// =====================================================================================================================
// Reverses the bytes on little-endian machine.  If the machine is big-endian, this function does nothing.
void ByteReverse(
    uint8* pBuf,   // [in, out] Buffer to reverse.
    uint32 longs)  // Number of dwords to reverse.
{
#if defined(LITTLEENDIAN_CPU)
    uint32 value;
    do
    {
        value = (pBuf[3] << 24) | (pBuf[2] << 16) | (pBuf[1] << 8) | pBuf[0];
        *reinterpret_cast<uint32*>(pBuf)= value;
        pBuf += 4;
    } while (--longs);
#else
    // No reversal is necessary on big-endian architecture.
#endif
}

} // Md5
} // Util
