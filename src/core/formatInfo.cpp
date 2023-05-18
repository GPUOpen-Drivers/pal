/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palFormatInfo.h"
#include "palDevice.h"
#include "palMath.h"

#include "core/g_mergedFormatInfo.h"
#include <cmath>

using namespace Util;
using namespace Util::Math;

namespace Pal
{
namespace Formats
{

static_assert(ArrayLen(FormatInfoTable) == static_cast<size_t>(ChNumFormat::Count),
    "Size of FormatInfoTable mismatches the number of declared ChNumFormat enums");

constexpr float LinearToGammaExponent  = 1.f / 2.4f;
constexpr float GammaToLinearExponent  = 2.4f;
constexpr float LinearToGammaScale1    = 1.055f;
constexpr float LinearToGammaScale2    = 12.92f;
constexpr float LinearToGammaOffset    = 0.055f;
constexpr float LinearToGammaThreshold = 0.0031308f;
constexpr float GammaToLinearThreshold = 0.04045f;

// =====================================================================================================================
// Converts a linearly-scaled color value to gamma-corrected sRGB. The conversion parameters are the same as documented
// in d3d10.h.
float LinearToGamma(
    float linear)
{
    float gamma;

    if (linear <= LinearToGammaThreshold)
    {
        gamma = (linear * LinearToGammaScale2);
    }
    else
    {
        const float temp = Pow(linear, LinearToGammaExponent);

        gamma = ((temp * LinearToGammaScale1) - LinearToGammaOffset);
    }

    return gamma;
}

// =====================================================================================================================
// Converts a gamma-corrected sRGB color value to linear color space. The conversion parameters are the same as
// documented in d3d10.h.
float GammaToLinear(
    float gammaCorrectedVal)
{
    float linearVal;

    if (gammaCorrectedVal <= GammaToLinearThreshold)
    {
        linearVal = (gammaCorrectedVal / LinearToGammaScale2);
    }
    else
    {
        float temp = (gammaCorrectedVal + LinearToGammaOffset);

        temp = (temp / LinearToGammaScale1);

        linearVal = Pow(temp, GammaToLinearExponent);
    }

    return linearVal;
}

// =====================================================================================================================
// Converts a color in RGB_ order into a shared exponent format, X9Y9Z9E5.
void ConvertColorToX9Y9Z9E5(
    const float*   pColorIn,
    uint32*        pColorOut)
{
    constexpr int32 MantissaBits      = 9;  // Number of mantissa bits per component
    constexpr int32 ExponentBias      = 15; // Exponent bias
    constexpr int32 MaxBiasedExponent = 31; // Maximum allowed biased exponent values
    constexpr int32 MantissaValues    = (1 << MantissaBits);

    constexpr float SharedExpMax = ((MantissaValues - 1) * (1 << (MaxBiasedExponent - MantissaBits))) / MantissaValues;

    // The RGB compenents are clamped
    const float redC   = Max(0.f, Min(SharedExpMax, pColorIn[0]));
    const float greenC = Max(0.f, Min(SharedExpMax, pColorIn[1]));
    const float blueC  = Max(0.f, Min(SharedExpMax, pColorIn[2]));

    // Find the largest clamped component
    const float maxC = Max(Max(redC, greenC), blueC);

    // Calculate a preliminary shared exponent
    int32 sharedExp = Max(-ExponentBias - 1, static_cast<int32>(floor(log2(maxC)))) + 1 + ExponentBias;
    PAL_ASSERT(sharedExp <= MaxBiasedExponent);

    float denom = pow(2.0f, static_cast<float>(sharedExp - ExponentBias - MantissaBits));

    // Max shared exponent RGB value
    const int32 maxS = static_cast<int32>(floor((maxC / denom) + 0.5f));

    // In the case where maxS == pow(2, MantissaBits) to fit everything into 9 bits we want to increase the shared
    // exponent and shrink the output RGB values.  Instead of recalculating what is done in the denom value with the
    // new sharedExp value, we just multiply it by 2 increasing the power of 2 it was raised to.
    if (maxS == MantissaValues)
    {
        denom *= 2;
        sharedExp++;
    }

    // Shared exponent RGB values
    const uint32 redS   = static_cast<uint32>(floor((redC   / denom) + 0.5f));
    const uint32 greenS = static_cast<uint32>(floor((greenC / denom) + 0.5f));
    const uint32 blueS  = static_cast<uint32>(floor((blueC  / denom) + 0.5f));

    pColorOut[0] = redS;
    pColorOut[1] = greenS;
    pColorOut[2] = blueS;
    pColorOut[3] = sharedExp;
}

// =====================================================================================================================
// Converts a color in RGBA order into X10Y10Z10W2.
void ConvertColorToX10Y10Z10W2(
    const float*   pColorIn,
    uint32*        pColorOut)
{
    pColorOut[0] = Float32ToFloat10_6e4(pColorIn[0]);
    pColorOut[1] = Float32ToFloat10_6e4(pColorIn[1]);
    pColorOut[2] = Float32ToFloat10_6e4(pColorIn[2]);
    pColorOut[3] = FloatToUFixed(pColorIn[3], 0, 2, true);
}

// =====================================================================================================================
// Converts a floating-point representation of a color value to the appropriate bit representation for each channel
// based on the specified format. This does not support the DepthStencilOnly or Undefined formats.
// RGBA order is expected and no swizzling is performed except to maintain backwards compatability.
void ConvertColor(
    SwizzledFormat format,
    const float*   pColorIn,
    uint32*        pColorOut)
{
    const FormatInfo& info = FormatInfoTable[static_cast<size_t>(format.format)];
    PAL_ASSERT(((info.properties & BitCountInaccurate) == 0) && (info.bitsPerPixel <= 128));

    if (format.format == ChNumFormat::X9Y9Z9E5_Float)
    {
        ConvertColorToX9Y9Z9E5(pColorIn, &pColorOut[0]);
    }
    else if (format.format == ChNumFormat::X10Y10Z10W2_Float)
    {
        ConvertColorToX10Y10Z10W2(pColorIn, &pColorOut[0]);
    }
    else
    {
        pColorOut[0] = 0;
        pColorOut[1] = 0;
        pColorOut[2] = 0;
        pColorOut[3] = 0;

        for (uint32 rgbaIdx = 0; rgbaIdx < 4; ++rgbaIdx)
        {
            // If this RGBA component maps to any of the components on the data format
            if ((format.swizzle.swizzle[rgbaIdx] >= ChannelSwizzle::X) &&
                (format.swizzle.swizzle[rgbaIdx] <= ChannelSwizzle::W))
            {
                // Map from RGBA to data format component index (compIdx = 0 = least-significant bit component)
                uint32 compIdx =
                    static_cast<uint32>(format.swizzle.swizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);

                // Get the number of bits of data format component using compIdx as there may be a swizzle
                const uint32 numBits = info.bitCount[compIdx];

                // Source RGBA component value
                const float rgbaVal = pColorIn[rgbaIdx];

                // Convert from RGBA float to data format component representation
                uint32 compVal;

                if (IsUnorm(format.format))
                {
                    compVal = FloatToUFixed(rgbaVal, 0, numBits, true);
                }
                else if (IsSnorm(format.format))
                {
                    compVal = FloatToSFixed(rgbaVal, 0, numBits, true);
                }
                else if (IsUscaled(format.format))
                {
                    compVal = FloatToUFixed(rgbaVal, numBits, 0, false);
                }
                else if (IsSscaled(format.format))
                {
                    compVal = FloatToSFixed(rgbaVal, numBits, 0, true);
                }
                else if (IsUint(format.format))
                {
                    // Integer conversion always truncates the fractional part
                    compVal = FloatToUFixed(rgbaVal, numBits, 0, false);
                }
                else if (IsSint(format.format))
                {
                    // Integer conversion always truncates the fractional part
                    compVal = FloatToSFixed(rgbaVal, numBits, 0, false);
                }
                else if (IsFloat(format.format))
                {
                    compVal = Float32ToNumBits(rgbaVal, numBits);
                }
                else if (IsSrgb(format.format))
                {
                    // sRGB conversions should never be applied to alpha channels.
                    if (rgbaIdx == 3)
                    {
                        compVal = FloatToUFixed(rgbaVal, 0, numBits, true);
                    }
                    else
                    {
                        compVal = FloatToUFixed(LinearToGamma(rgbaVal), 0, numBits, true);
                    }
                }
                else
                {
                    PAL_ASSERT_ALWAYS();
                    compVal = 0;
                }

                // Write the converted value without swizzling
                pColorOut[rgbaIdx] = compVal;
            }
        }
    }
}

// =====================================================================================================================
// Converts an unsigned integer representation of a color value YUVA order to the appropriate bit representation for
// each channel based on the specified format.
void ConvertYuvColor(
    SwizzledFormat format,
    uint32         plane,
    const uint32*  pColorIn,
    uint32*        pColorOut)
{
    switch (format.format)
    {
    case ChNumFormat::AYUV:
        // The order of AYUV is actually VUYA
        pColorOut[0] = (pColorIn[2] | (pColorIn[1] << 8) | (pColorIn[0] << 16) | (pColorIn[3] << 24));
        break;
    case ChNumFormat::UYVY:
        pColorOut[0] = (pColorIn[1] | (pColorIn[0] << 8) | (pColorIn[2] << 16) | (pColorIn[0] << 24));
        break;
    case ChNumFormat::VYUY:
        pColorOut[0] = (pColorIn[2] | (pColorIn[0] << 8) | (pColorIn[1] << 16) | (pColorIn[0] << 24));
        break;
    case ChNumFormat::YUY2:
        pColorOut[0] = (pColorIn[0] | (pColorIn[1] << 8) | (pColorIn[0] << 16) | (pColorIn[2] << 24));
        break;
    case ChNumFormat::YVY2:
        pColorOut[0] = (pColorIn[0] | (pColorIn[2] << 8) | (pColorIn[0] << 16) | (pColorIn[1] << 24));
        break;
    case ChNumFormat::P412:
    case ChNumFormat::YV12:
        if (plane == 0)
        {
            pColorOut[0] = pColorIn[0];
        }
        else if (plane == 1)
        {
            pColorOut[0] = pColorIn[1];
        }
        else if (plane == 2)
        {
            pColorOut[0] = pColorIn[2];
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;
    case ChNumFormat::NV11:
    case ChNumFormat::NV12:
    case ChNumFormat::P208:
        if (plane == 0)
        {
            pColorOut[0] = pColorIn[0];
        }
        else if (plane == 1)
        {
            pColorOut[0] = (pColorIn[1] | (pColorIn[2] << 8));
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;
    case ChNumFormat::NV21:
        if (plane == 0)
        {
            pColorOut[0] = pColorIn[0];
        }
        else if (plane == 1)
        {
            pColorOut[0] = (pColorIn[2] | (pColorIn[1] << 8));
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;
    case ChNumFormat::P016:
    case ChNumFormat::P010:
    case ChNumFormat::P210:
    case ChNumFormat::P012:
    case ChNumFormat::P212:
        if (plane == 0)
        {
            pColorOut[0] = pColorIn[0];
        }
        else if (plane == 1)
        {
            pColorOut[0] = (pColorIn[1] | (pColorIn[2] << 16));
        }
        else
        {
            PAL_ASSERT_ALWAYS();
        }
        break;
    case ChNumFormat::Y216:
    case ChNumFormat::Y210:
        pColorOut[0] = (pColorIn[0] | (pColorIn[1] << 16));
        pColorOut[1] = (pColorIn[0] | (pColorIn[2] << 16));
        break;
    case ChNumFormat::Y416:
        pColorOut[0] = (pColorIn[1] | (pColorIn[0] << 16));
        pColorOut[1] = (pColorIn[2] | (pColorIn[3] << 16));
        break;
    case ChNumFormat::Y410:
        pColorOut[0] = (pColorIn[1] | (pColorIn[0] << 10) | (pColorIn[2] << 20) | (pColorIn[3] << 30));
        break;
    default:
        PAL_ASSERT_ALWAYS();
        break;
    }
}

// =====================================================================================================================
// Packs the raw clear color into a single element of the provided format and stores it in the memory provided.
// RGBA order is expected and no swizzling is performed except to maintain backwards compatability. A clear color
// should never be swizzled after it is packed.
void PackRawClearColor(
    SwizzledFormat format,
    const uint32*  pColor,
    void*          pBufferMemory)
{
    // This function relies on the component bit counts being accurate, and assumes a max of 4 DWORD components.
    const auto& info = FormatInfoTable[static_cast<size_t>(format.format)];
    PAL_ASSERT(((info.properties & BitCountInaccurate) == 0) && (info.bitsPerPixel <= 128));

    uint32 packedColor[4] = {};
    uint32 bitCount       = 0;
    uint32 dwordCount     = 0;

    for (uint32 compIdx = 0; compIdx < 4; compIdx++)
    {
        const uint32 compBitCount = info.bitCount[compIdx];
        if (compBitCount > 0)
        {
            const uint64 mask = ((1ull << compBitCount) - 1ull) << bitCount;

            packedColor[dwordCount] &= ~mask;
            packedColor[dwordCount] |= ((static_cast<uint64>(pColor[compIdx]) << bitCount) & mask);

            bitCount += compBitCount;
            PAL_ASSERT(bitCount <= 32);

            if (bitCount == 32)
            {
                dwordCount++;
                bitCount = 0;
            }
        }
    }

    // Copy the packed values into buffer memory.
    memcpy(pBufferMemory, &packedColor[0], BytesPerPixel(format.format));
}

// =====================================================================================================================
// Swizzles the color according to the provided format.
void SwizzleColor(
    SwizzledFormat format,
    const uint32*  pColorIn,
    uint32*        pColorOut)
{
    pColorOut[0] = 0;
    pColorOut[1] = 0;
    pColorOut[2] = 0;
    pColorOut[3] = 0;

    for (uint32 rgbaIdx = 0; rgbaIdx < 4; ++rgbaIdx)
    {
        // If this RGBA component maps to any of the components on the data format
        if ((format.swizzle.swizzle[rgbaIdx] >= ChannelSwizzle::X) &&
            (format.swizzle.swizzle[rgbaIdx] <= ChannelSwizzle::W))
        {
            uint32 compIdx =
                static_cast<uint32>(format.swizzle.swizzle[rgbaIdx]) - static_cast<uint32>(ChannelSwizzle::X);
            pColorOut[compIdx] = pColorIn[rgbaIdx];
        }
        else if (format.format == ChNumFormat::X9Y9Z9E5_Float)
        {
            pColorOut[rgbaIdx] = pColorIn[rgbaIdx];
        }
    }
}

// =====================================================================================================================
// Converts format into its Unorm equivalent.
ChNumFormat PAL_STDCALL ConvertToUnorm(
    ChNumFormat format)
{
    constexpr ChNumFormat UnormTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::X1_Unorm,              // ChNumFormat::X1_Unorm
        ChNumFormat::X1_Unorm,              // ChNumFormat::X1_Uscaled
        ChNumFormat::X4Y4_Unorm,            // ChNumFormat::X4Y4_Unorm
        ChNumFormat::X4Y4_Unorm,            // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::L4A4_Unorm,            // ChNumFormat::L4A4_Unorm
        ChNumFormat::X4Y4Z4W4_Unorm,        // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::X4Y4Z4W4_Unorm,        // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::X5Y6Z5_Unorm,          // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::X5Y6Z5_Unorm,          // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::X5Y5Z5W1_Unorm,        // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::X5Y5Z5W1_Unorm,        // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::X1Y5Z5W5_Unorm,        // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::X1Y5Z5W5_Unorm,        // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Uint
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Sint
        ChNumFormat::X8_Unorm,              // ChNumFormat::X8_Srgb
        ChNumFormat::A8_Unorm,              // ChNumFormat::A8_Unorm
        ChNumFormat::L8_Unorm,              // ChNumFormat::L8_Unorm
        ChNumFormat::P8_Unorm,              // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Unorm,            // ChNumFormat::X8Y8_Srgb
        ChNumFormat::L8A8_Unorm,            // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Unorm,        // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Uint
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Sint
        ChNumFormat::X16_Unorm,             // ChNumFormat::X16_Float
        ChNumFormat::L16_Unorm,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Unorm,          // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Unorm,    // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Bc1_Unorm,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Bc1_Unorm,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Bc2_Unorm,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Bc2_Unorm,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Bc3_Unorm,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Bc3_Unorm,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Bc4_Unorm,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Bc4_Unorm,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Bc5_Unorm,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Bc5_Unorm,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Bc7_Unorm,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Bc7_Unorm,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Etc2X8Y8Z8_Unorm,      // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Etc2X8Y8Z8_Unorm,      // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,    // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Etc2X8Y8Z8W1_Unorm,    // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,    // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Etc2X8Y8Z8W8_Unorm,    // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Etc2X11_Unorm,         // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Etc2X11_Unorm,         // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Etc2X11Y11_Unorm,      // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Etc2X11Y11_Unorm,      // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::AstcLdr4x4_Unorm,      // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::AstcLdr4x4_Unorm,      // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::AstcLdr5x4_Unorm,      // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::AstcLdr5x4_Unorm,      // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::AstcLdr5x5_Unorm,      // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::AstcLdr5x5_Unorm,      // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::AstcLdr6x5_Unorm,      // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::AstcLdr6x5_Unorm,      // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::AstcLdr6x6_Unorm,      // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::AstcLdr6x6_Unorm,      // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::AstcLdr8x5_Unorm,      // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::AstcLdr8x5_Unorm,      // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::AstcLdr8x6_Unorm,      // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::AstcLdr8x6_Unorm,      // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::AstcLdr8x8_Unorm,      // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::AstcLdr8x8_Unorm,      // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::AstcLdr10x5_Unorm,     // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::AstcLdr10x5_Unorm,     // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::AstcLdr10x6_Unorm,     // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::AstcLdr10x6_Unorm,     // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::AstcLdr10x8_Unorm,     // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::AstcLdr10x8_Unorm,     // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::AstcLdr10x10_Unorm,    // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::AstcLdr10x10_Unorm,    // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::AstcLdr12x10_Unorm,    // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::AstcLdr12x10_Unorm,    // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::AstcLdr12x12_Unorm,    // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::AstcLdr12x12_Unorm,    // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::X8Y8_Z8Y8_Unorm,       // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::X8Y8_Z8Y8_Unorm,       // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Y8X8_Y8Z8_Unorm,       // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Y8X8_Y8Z8_Unorm,       // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::X8_MM_Unorm,           // ChNumFormat::X8_MM_Unorm
        ChNumFormat::X8_MM_Unorm,           // ChNumFormat::X8_MM_Uint
        ChNumFormat::X8Y8_MM_Unorm,         // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::X8Y8_MM_Unorm,         // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::X16_MM10_Unorm,        // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::X16_MM10_Unorm,        // ChNumFormat::X16_MM10_Uint
        ChNumFormat::X16Y16_MM10_Unorm,     // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::X16Y16_MM10_Unorm,     // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::X16_MM12_Unorm,        // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::X16_MM12_Unorm,        // ChNumFormat::X16_MM12_Uint
        ChNumFormat::X16Y16_MM12_Unorm,     // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::X16Y16_MM12_Unorm,     // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Unorm,     // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(UnormTable) == static_cast<size_t>(ChNumFormat::Count),
                  "UnormTable does not match the number of ChNumFormats!");

    return UnormTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Snorm equivalent.
ChNumFormat PAL_STDCALL ConvertToSnorm(
    ChNumFormat format)
{
    constexpr ChNumFormat SnormTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Uint
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Sint
        ChNumFormat::X8_Snorm,              // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Snorm,            // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Snorm,        // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Uint
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Sint
        ChNumFormat::X16_Snorm,             // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Snorm,          // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Snorm,    // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Bc4_Snorm,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Bc4_Snorm,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Bc5_Snorm,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Bc5_Snorm,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Etc2X11_Snorm,         // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Etc2X11_Snorm,         // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Etc2X11Y11_Snorm,      // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Etc2X11Y11_Snorm,      // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Snorm,     // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(SnormTable) == static_cast<size_t>(ChNumFormat::Count),
                  "SnormTable does not match the number of ChNumFormats!");

    return SnormTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Uscaled equivalent.
ChNumFormat PAL_STDCALL ConvertToUscaled(
    ChNumFormat format)
{
    constexpr ChNumFormat UscaledTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::X1_Uscaled,            // ChNumFormat::X1_Unorm
        ChNumFormat::X1_Uscaled,            // ChNumFormat::X1_Uscaled
        ChNumFormat::X4Y4_Uscaled,          // ChNumFormat::X4Y4_Unorm
        ChNumFormat::X4Y4_Uscaled,          // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::X4Y4Z4W4_Uscaled,      // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::X4Y4Z4W4_Uscaled,      // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::X5Y6Z5_Uscaled,        // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::X5Y6Z5_Uscaled,        // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::X5Y5Z5W1_Uscaled,      // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::X5Y5Z5W1_Uscaled,      // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::X1Y5Z5W5_Uscaled,      // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::X1Y5Z5W5_Uscaled,      // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Uint
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Sint
        ChNumFormat::X8_Uscaled,            // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Uscaled,          // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Uscaled,      // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Uint
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Sint
        ChNumFormat::X16_Uscaled,           // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Uscaled,        // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Uscaled,  // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::X8Y8_Z8Y8_Uscaled,     // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::X8Y8_Z8Y8_Uscaled,     // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Y8X8_Y8Z8_Uscaled,     // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Y8X8_Y8Z8_Uscaled,     // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Uscaled,   // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(UscaledTable) == static_cast<size_t>(ChNumFormat::Count),
                  "UscaledTable does not match the number of ChNumFormats!");

    return UscaledTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Sscaled equivalent.
ChNumFormat PAL_STDCALL ConvertToSscaled(
    ChNumFormat format)
{
    constexpr ChNumFormat SscaledTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Uint
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Sint
        ChNumFormat::X8_Sscaled,            // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Sscaled,          // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Sscaled,      // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Uint
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Sint
        ChNumFormat::X16_Sscaled,           // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Sscaled,        // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Sscaled,  // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Sscaled,   // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(SscaledTable) == static_cast<size_t>(ChNumFormat::Count),
                  "SscaledTable does not match the number of ChNumFormats!");

    return SscaledTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Uint equivalent.
ChNumFormat PAL_STDCALL ConvertToUint(
    ChNumFormat format)
{
    constexpr ChNumFormat UintTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Uint
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Sint
        ChNumFormat::X8_Uint,               // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Uint,             // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Uint,         // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Uint
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Sint
        ChNumFormat::X16_Uint,              // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Uint,           // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Uint,     // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::X32_Uint,              // ChNumFormat::X32_Uint
        ChNumFormat::X32_Uint,              // ChNumFormat::X32_Sint
        ChNumFormat::X32_Uint,              // ChNumFormat::X32_Float
        ChNumFormat::X32Y32_Uint,           // ChNumFormat::X32Y32_Uint
        ChNumFormat::X32Y32_Uint,           // ChNumFormat::X32Y32_Sint
        ChNumFormat::X32Y32_Uint,           // ChNumFormat::X32Y32_Float
        ChNumFormat::X32Y32Z32_Uint,        // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::X32Y32Z32_Uint,        // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::X32Y32Z32_Uint,        // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::X32Y32Z32W32_Uint,     // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::X32Y32Z32W32_Uint,     // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::X32Y32Z32W32_Uint,     // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::X8_MM_Uint,            // ChNumFormat::X8_MM_Unorm
        ChNumFormat::X8_MM_Uint,            // ChNumFormat::X8_MM_Uint
        ChNumFormat::X8Y8_MM_Uint,          // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::X8Y8_MM_Uint,          // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::X16_MM10_Uint,         // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::X16_MM10_Uint,         // ChNumFormat::X16_MM10_Uint
        ChNumFormat::X16Y16_MM10_Uint,      // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::X16Y16_MM10_Uint,      // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::X16_MM12_Uint,         // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::X16_MM12_Uint,         // ChNumFormat::X16_MM12_Uint
        ChNumFormat::X16Y16_MM12_Uint,      // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::X16Y16_MM12_Uint,      // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Uint,      // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(UintTable) == static_cast<size_t>(ChNumFormat::Count),
                  "UintTable does not match the number of ChNumFormats!");

    return UintTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Sint equivalent.
ChNumFormat PAL_STDCALL ConvertToSint(
    ChNumFormat format)
{
    constexpr ChNumFormat SintTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Uint
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Sint
        ChNumFormat::X8_Sint,               // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Sint,             // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Sint,         // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Uint
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Sint
        ChNumFormat::X16_Sint,              // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Sint,           // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Sint,     // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::X32_Sint,              // ChNumFormat::X32_Uint
        ChNumFormat::X32_Sint,              // ChNumFormat::X32_Sint
        ChNumFormat::X32_Sint,              // ChNumFormat::X32_Float
        ChNumFormat::X32Y32_Sint,           // ChNumFormat::X32Y32_Uint
        ChNumFormat::X32Y32_Sint,           // ChNumFormat::X32Y32_Sint
        ChNumFormat::X32Y32_Sint,           // ChNumFormat::X32Y32_Float
        ChNumFormat::X32Y32Z32_Sint,        // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::X32Y32Z32_Sint,        // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::X32Y32Z32_Sint,        // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::X32Y32Z32W32_Sint,     // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::X32Y32Z32W32_Sint,     // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::X32Y32Z32W32_Sint,     // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Sint,      // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(SintTable) == static_cast<size_t>(ChNumFormat::Count),
                  "SintTable does not match the number of ChNumFormats!");

    return SintTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Float equivalent.
ChNumFormat PAL_STDCALL ConvertToFloat(
    ChNumFormat format)
{
    constexpr ChNumFormat FloatTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::X10Y11Z11_Float,       // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::X11Y11Z10_Float,       // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Unorm
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Snorm
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Uscaled
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Sscaled
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Uint
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Sint
        ChNumFormat::X16_Float,             // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Unorm
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Snorm
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Uint
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Sint
        ChNumFormat::X16Y16_Float,          // ChNumFormat::X16Y16_Float
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::X16Y16Z16W16_Float,    // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::X32_Float,             // ChNumFormat::X32_Uint
        ChNumFormat::X32_Float,             // ChNumFormat::X32_Sint
        ChNumFormat::X32_Float,             // ChNumFormat::X32_Float
        ChNumFormat::X32Y32_Float,          // ChNumFormat::X32Y32_Uint
        ChNumFormat::X32Y32_Float,          // ChNumFormat::X32Y32_Sint
        ChNumFormat::X32Y32_Float,          // ChNumFormat::X32Y32_Float
        ChNumFormat::X32Y32Z32_Float,       // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::X32Y32Z32_Float,       // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::X32Y32Z32_Float,       // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::X32Y32Z32W32_Float,    // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::X32Y32Z32W32_Float,    // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::X32Y32Z32W32_Float,    // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::X9Y9Z9E5_Float,        // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc2_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Bc6_Ufloat,            // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Bc6_Sfloat,            // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc7_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::AstcHdr4x4_Float,      // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::AstcHdr5x4_Float,      // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::AstcHdr5x5_Float,      // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::AstcHdr6x5_Float,      // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::AstcHdr6x6_Float,      // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::AstcHdr8x5_Float,      // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::AstcHdr8x6_Float,      // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::AstcHdr8x8_Float,      // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::AstcHdr10x5_Float,     // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::AstcHdr10x6_Float,     // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::AstcHdr10x8_Float,     // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::AstcHdr10x10_Float,    // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::AstcHdr12x10_Float,    // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::AstcHdr12x12_Float,    // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::X10Y10Z10W2_Float,     // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(FloatTable) == static_cast<size_t>(ChNumFormat::Count),
                  "FloatTable does not match the number of ChNumFormats!");

    return FloatTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts format into its Srgb equivalent.
ChNumFormat PAL_STDCALL ConvertToSrgb(
    ChNumFormat format)
{
    constexpr ChNumFormat SrgbTable[] =
    {
        ChNumFormat::Undefined,             // ChNumFormat::Undefined
        ChNumFormat::Undefined,             // ChNumFormat::X1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::L4A4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X4Y4Z4W4_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y6Z5_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X5Y5Z5W1_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X1Y5Z5W5_Uscaled
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Unorm
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Snorm
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Uscaled
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Sscaled
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Uint
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Sint
        ChNumFormat::X8_Srgb,               // ChNumFormat::X8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::A8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::L8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::P8_Unorm
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Unorm
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Snorm
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Uscaled
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Sscaled
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Uint
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Sint
        ChNumFormat::X8Y8_Srgb,             // ChNumFormat::X8Y8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::L8A8_Unorm
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Unorm
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Snorm
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Uscaled
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Sscaled
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Uint
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Sint
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::X8Y8Z8W8_Srgb
        ChNumFormat::X8Y8Z8W8_Srgb,         // ChNumFormat::U8V8_Snorm_L8W8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y11Z11_Float
        ChNumFormat::Undefined,             // ChNumFormat::X11Y11Z10_Float
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2Bias_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::U10V10W10_Snorm_A2_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X16_Float
        ChNumFormat::Undefined,             // ChNumFormat::L16_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Sscaled
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16Z16W16_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32_Float
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Sint
        ChNumFormat::Undefined,             // ChNumFormat::X32Y32Z32W32_Float
        ChNumFormat::Undefined,             // ChNumFormat::D16_Unorm_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::D32_Float_S8_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X9Y9Z9E5_Float
        ChNumFormat::Bc1_Srgb,              // ChNumFormat::Bc1_Unorm
        ChNumFormat::Bc1_Srgb,              // ChNumFormat::Bc1_Srgb
        ChNumFormat::Bc2_Srgb,              // ChNumFormat::Bc2_Unorm
        ChNumFormat::Bc2_Srgb,              // ChNumFormat::Bc2_Srgb
        ChNumFormat::Bc3_Srgb,              // ChNumFormat::Bc3_Unorm
        ChNumFormat::Bc3_Srgb,              // ChNumFormat::Bc3_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc4_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc5_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Ufloat
        ChNumFormat::Undefined,             // ChNumFormat::Bc6_Sfloat
        ChNumFormat::Bc7_Srgb,              // ChNumFormat::Bc7_Unorm
        ChNumFormat::Bc7_Srgb,              // ChNumFormat::Bc7_Srgb
        ChNumFormat::Etc2X8Y8Z8_Srgb,       // ChNumFormat::Etc2X8Y8Z8_Unorm
        ChNumFormat::Etc2X8Y8Z8_Srgb,       // ChNumFormat::Etc2X8Y8Z8_Srgb
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,     // ChNumFormat::Etc2X8Y8Z8W1_Unorm
        ChNumFormat::Etc2X8Y8Z8W1_Srgb,     // ChNumFormat::Etc2X8Y8Z8W1_Srgb
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,     // ChNumFormat::Etc2X8Y8Z8W8_Unorm
        ChNumFormat::Etc2X8Y8Z8W8_Srgb,     // ChNumFormat::Etc2X8Y8Z8W8_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11_Snorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Etc2X11Y11_Snorm
        ChNumFormat::AstcLdr4x4_Srgb,       // ChNumFormat::AstcLdr4x4_Unorm
        ChNumFormat::AstcLdr4x4_Srgb,       // ChNumFormat::AstcLdr4x4_Srgb
        ChNumFormat::AstcLdr5x4_Srgb,       // ChNumFormat::AstcLdr5x4_Unorm
        ChNumFormat::AstcLdr5x4_Srgb,       // ChNumFormat::AstcLdr5x4_Srgb
        ChNumFormat::AstcLdr5x5_Srgb,       // ChNumFormat::AstcLdr5x5_Unorm
        ChNumFormat::AstcLdr5x5_Srgb,       // ChNumFormat::AstcLdr5x5_Srgb
        ChNumFormat::AstcLdr6x5_Srgb,       // ChNumFormat::AstcLdr6x5_Unorm
        ChNumFormat::AstcLdr6x5_Srgb,       // ChNumFormat::AstcLdr6x5_Srgb
        ChNumFormat::AstcLdr6x6_Srgb,       // ChNumFormat::AstcLdr6x6_Unorm
        ChNumFormat::AstcLdr6x6_Srgb,       // ChNumFormat::AstcLdr6x6_Srgb
        ChNumFormat::AstcLdr8x5_Srgb,       // ChNumFormat::AstcLdr8x5_Unorm
        ChNumFormat::AstcLdr8x5_Srgb,       // ChNumFormat::AstcLdr8x5_Srgb
        ChNumFormat::AstcLdr8x6_Srgb,       // ChNumFormat::AstcLdr8x6_Unorm
        ChNumFormat::AstcLdr8x6_Srgb,       // ChNumFormat::AstcLdr8x6_Srgb
        ChNumFormat::AstcLdr8x8_Srgb,       // ChNumFormat::AstcLdr8x8_Unorm
        ChNumFormat::AstcLdr8x8_Srgb,       // ChNumFormat::AstcLdr8x8_Srgb
        ChNumFormat::AstcLdr10x5_Srgb,      // ChNumFormat::AstcLdr10x5_Unorm
        ChNumFormat::AstcLdr10x5_Srgb,      // ChNumFormat::AstcLdr10x5_Srgb
        ChNumFormat::AstcLdr10x6_Srgb,      // ChNumFormat::AstcLdr10x6_Unorm
        ChNumFormat::AstcLdr10x6_Srgb,      // ChNumFormat::AstcLdr10x6_Srgb
        ChNumFormat::AstcLdr10x8_Srgb,      // ChNumFormat::AstcLdr10x8_Unorm
        ChNumFormat::AstcLdr10x8_Srgb,      // ChNumFormat::AstcLdr10x8_Srgb
        ChNumFormat::AstcLdr10x10_Srgb,     // ChNumFormat::AstcLdr10x10_Unorm
        ChNumFormat::AstcLdr10x10_Srgb,     // ChNumFormat::AstcLdr10x10_Srgb
        ChNumFormat::AstcLdr12x10_Srgb,     // ChNumFormat::AstcLdr12x10_Unorm
        ChNumFormat::AstcLdr12x10_Srgb,     // ChNumFormat::AstcLdr12x10_Srgb
        ChNumFormat::AstcLdr12x12_Srgb,     // ChNumFormat::AstcLdr12x12_Unorm
        ChNumFormat::AstcLdr12x12_Srgb,     // ChNumFormat::AstcLdr12x12_Srgb
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr4x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x4_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr5x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr6x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr8x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x5_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x6_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x8_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr10x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x10_Float
        ChNumFormat::Undefined,             // ChNumFormat::AstcHdr12x12_Float
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_Z8Y8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::Y8X8_Y8Z8_Uscaled
        ChNumFormat::Undefined,             // ChNumFormat::AYUV
        ChNumFormat::Undefined,             // ChNumFormat::UYVY
        ChNumFormat::Undefined,             // ChNumFormat::VYUY
        ChNumFormat::Undefined,             // ChNumFormat::YUY2
        ChNumFormat::Undefined,             // ChNumFormat::YVY2
        ChNumFormat::Undefined,             // ChNumFormat::YV12
        ChNumFormat::Undefined,             // ChNumFormat::NV11
        ChNumFormat::Undefined,             // ChNumFormat::NV12
        ChNumFormat::Undefined,             // ChNumFormat::NV21
        ChNumFormat::Undefined,             // ChNumFormat::P016
        ChNumFormat::Undefined,             // ChNumFormat::P010
        ChNumFormat::Undefined,             // ChNumFormat::P210
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X8Y8_MM_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM10_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P208
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Unorm
        ChNumFormat::Undefined,             // ChNumFormat::X16Y16_MM12_Uint
        ChNumFormat::Undefined,             // ChNumFormat::P012
        ChNumFormat::Undefined,             // ChNumFormat::P212
        ChNumFormat::Undefined,             // ChNumFormat::P412
        ChNumFormat::Undefined,             // ChNumFormat::X10Y10Z10W2_Float
        ChNumFormat::Undefined,             // ChNumFormat::Y216
        ChNumFormat::Undefined,             // ChNumFormat::Y210
        ChNumFormat::Undefined,             // ChNumFormat::Y416
        ChNumFormat::Undefined,             // ChNumFormat::Y410
    };

    static_assert(ArrayLen(SrgbTable) == static_cast<size_t>(ChNumFormat::Count),
                  "SrgbTable does not match the number of ChNumFormats!");

    return SrgbTable[static_cast<size_t>(format)];
};

// =====================================================================================================================
// Converts source numeric format to the provided destination numeric format.
ChNumFormat ConvertToDstNumFmt(
    ChNumFormat srcFormat,
    ChNumFormat dstFormat)
{
    ChNumFormat format = ChNumFormat::Undefined;

    switch (FormatInfoTable[static_cast<size_t>(dstFormat)].numericSupport)
    {
    case NumericSupportFlags::Unorm:
        format = ConvertToUnorm(srcFormat);
        break;
    case NumericSupportFlags::Snorm:
        format = ConvertToSnorm(srcFormat);
        break;
    case NumericSupportFlags::Uscaled:
        format = ConvertToUscaled(srcFormat);
        break;
    case NumericSupportFlags::Sscaled:
        format = ConvertToSscaled(srcFormat);
        break;
    case NumericSupportFlags::Uint:
        format = ConvertToUint(srcFormat);
        break;
    case NumericSupportFlags::Sint:
        format = ConvertToSint(srcFormat);
        break;
    case NumericSupportFlags::Float:
        format = ConvertToFloat(srcFormat);
        break;
    case NumericSupportFlags::Srgb:
        format = ConvertToSrgb(srcFormat);
        break;
    case NumericSupportFlags::DepthStencil:
    case NumericSupportFlags::Yuv:
    case NumericSupportFlags::Undefined:
    default:
        PAL_ASSERT(srcFormat == dstFormat);
        format = srcFormat;
        break;
    }

    return format;
}

// =====================================================================================================================
// Determines whether the srcFormat and the dstFormat have the same channel formats.
bool ShareChFmt(
    ChNumFormat srcFormat,
    ChNumFormat dstFormat)
{
    bool isSame = false;

    switch (srcFormat)
    {
    case ChNumFormat::Undefined:
    case ChNumFormat::L4A4_Unorm:
    case ChNumFormat::A8_Unorm:
    case ChNumFormat::L8_Unorm:
    case ChNumFormat::P8_Unorm:
    case ChNumFormat::L8A8_Unorm:
    case ChNumFormat::X10Y11Z11_Float:
    case ChNumFormat::X11Y11Z10_Float:
    case ChNumFormat::L16_Unorm:
    case ChNumFormat::D16_Unorm_S8_Uint:
    case ChNumFormat::D32_Float_S8_Uint:
    case ChNumFormat::X9Y9Z9E5_Float:
    case ChNumFormat::AstcHdr4x4_Float:
    case ChNumFormat::AstcHdr5x4_Float:
    case ChNumFormat::AstcHdr5x5_Float:
    case ChNumFormat::AstcHdr6x5_Float:
    case ChNumFormat::AstcHdr6x6_Float:
    case ChNumFormat::AstcHdr8x5_Float:
    case ChNumFormat::AstcHdr8x6_Float:
    case ChNumFormat::AstcHdr8x8_Float:
    case ChNumFormat::AstcHdr10x5_Float:
    case ChNumFormat::AstcHdr10x6_Float:
    case ChNumFormat::AstcHdr10x8_Float:
    case ChNumFormat::AstcHdr10x10_Float:
    case ChNumFormat::AstcHdr12x10_Float:
    case ChNumFormat::AstcHdr12x12_Float:
    case ChNumFormat::AYUV:
    case ChNumFormat::UYVY:
    case ChNumFormat::VYUY:
    case ChNumFormat::YUY2:
    case ChNumFormat::YVY2:
    case ChNumFormat::YV12:
    case ChNumFormat::NV11:
    case ChNumFormat::NV12:
    case ChNumFormat::NV21:
    case ChNumFormat::P016:
    case ChNumFormat::P010:
    case ChNumFormat::P208:
    case ChNumFormat::P210:
    case ChNumFormat::P012:
    case ChNumFormat::P212:
    case ChNumFormat::P412:
    case ChNumFormat::U8V8_Snorm_L8W8_Unorm:
    case ChNumFormat::U10V10W10_Snorm_A2_Unorm:
    case ChNumFormat::Y216:
    case ChNumFormat::Y210:
    case ChNumFormat::Y416:
    case ChNumFormat::Y410:
        isSame = (srcFormat == dstFormat);
        break;
    case ChNumFormat::X1_Unorm:
    case ChNumFormat::X1_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X1_Unorm) || (dstFormat == ChNumFormat::X1_Uscaled));
        break;
    case ChNumFormat::X4Y4_Unorm:
    case ChNumFormat::X4Y4_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X4Y4_Unorm) || (dstFormat == ChNumFormat::X4Y4_Uscaled));
        break;
    case ChNumFormat::X4Y4Z4W4_Unorm:
    case ChNumFormat::X4Y4Z4W4_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X4Y4Z4W4_Unorm) || (dstFormat == ChNumFormat::X4Y4Z4W4_Uscaled));
        break;
    case ChNumFormat::X5Y6Z5_Unorm:
    case ChNumFormat::X5Y6Z5_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X5Y6Z5_Unorm) || (dstFormat == ChNumFormat::X5Y6Z5_Uscaled));
        break;
    case ChNumFormat::X5Y5Z5W1_Unorm:
    case ChNumFormat::X5Y5Z5W1_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X5Y5Z5W1_Unorm) || (dstFormat == ChNumFormat::X5Y5Z5W1_Uscaled));
        break;
    case ChNumFormat::X1Y5Z5W5_Unorm:
    case ChNumFormat::X1Y5Z5W5_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X1Y5Z5W5_Unorm) || (dstFormat == ChNumFormat::X1Y5Z5W5_Uscaled));
        break;
    case ChNumFormat::X8_Unorm:
    case ChNumFormat::X8_Snorm:
    case ChNumFormat::X8_Uscaled:
    case ChNumFormat::X8_Sscaled:
    case ChNumFormat::X8_Uint:
    case ChNumFormat::X8_Sint:
    case ChNumFormat::X8_Srgb:
    case ChNumFormat::X8_MM_Unorm:
    case ChNumFormat::X8_MM_Uint:
        isSame = ((dstFormat == ChNumFormat::X8_Unorm)   || (dstFormat == ChNumFormat::X8_Snorm)    ||
                  (dstFormat == ChNumFormat::X8_Uscaled) || (dstFormat == ChNumFormat::X8_Sscaled)  ||
                  (dstFormat == ChNumFormat::X8_Uint)    || (dstFormat == ChNumFormat::X8_Sint)     ||
                  (dstFormat == ChNumFormat::X8_Srgb)    || (dstFormat == ChNumFormat::X8_MM_Unorm) ||
                  (dstFormat == ChNumFormat::X8_MM_Uint));
        break;
    case ChNumFormat::X8Y8_Unorm:
    case ChNumFormat::X8Y8_Snorm:
    case ChNumFormat::X8Y8_Uscaled:
    case ChNumFormat::X8Y8_Sscaled:
    case ChNumFormat::X8Y8_Uint:
    case ChNumFormat::X8Y8_Sint:
    case ChNumFormat::X8Y8_Srgb:
    case ChNumFormat::X8Y8_MM_Unorm:
    case ChNumFormat::X8Y8_MM_Uint:
        isSame = ((dstFormat == ChNumFormat::X8Y8_Unorm)   || (dstFormat == ChNumFormat::X8Y8_Snorm)    ||
                  (dstFormat == ChNumFormat::X8Y8_Uscaled) || (dstFormat == ChNumFormat::X8Y8_Sscaled)  ||
                  (dstFormat == ChNumFormat::X8Y8_Uint)    || (dstFormat == ChNumFormat::X8Y8_Sint)     ||
                  (dstFormat == ChNumFormat::X8Y8_Srgb)    || (dstFormat == ChNumFormat::X8Y8_MM_Unorm) ||
                  (dstFormat == ChNumFormat::X8Y8_MM_Uint));
        break;
    case ChNumFormat::X8Y8Z8W8_Unorm:
    case ChNumFormat::X8Y8Z8W8_Snorm:
    case ChNumFormat::X8Y8Z8W8_Uscaled:
    case ChNumFormat::X8Y8Z8W8_Sscaled:
    case ChNumFormat::X8Y8Z8W8_Uint:
    case ChNumFormat::X8Y8Z8W8_Sint:
    case ChNumFormat::X8Y8Z8W8_Srgb:
        isSame = ((dstFormat == ChNumFormat::X8Y8Z8W8_Unorm)   || (dstFormat == ChNumFormat::X8Y8Z8W8_Snorm)   ||
                  (dstFormat == ChNumFormat::X8Y8Z8W8_Uscaled) || (dstFormat == ChNumFormat::X8Y8Z8W8_Sscaled) ||
                  (dstFormat == ChNumFormat::X8Y8Z8W8_Uint)    || (dstFormat == ChNumFormat::X8Y8Z8W8_Sint)    ||
                  (dstFormat == ChNumFormat::X8Y8Z8W8_Srgb));
        break;
    case ChNumFormat::X10Y10Z10W2_Unorm:
    case ChNumFormat::X10Y10Z10W2_Snorm:
    case ChNumFormat::X10Y10Z10W2_Uscaled:
    case ChNumFormat::X10Y10Z10W2_Sscaled:
    case ChNumFormat::X10Y10Z10W2_Uint:
    case ChNumFormat::X10Y10Z10W2_Sint:
    case ChNumFormat::X10Y10Z10W2_Float:
    case ChNumFormat::X10Y10Z10W2Bias_Unorm:
        isSame = ((dstFormat == ChNumFormat::X10Y10Z10W2_Unorm)   ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Snorm)   ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Uscaled) ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Sscaled) ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Uint)    ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Sint)    ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2_Float)   ||
                  (dstFormat == ChNumFormat::X10Y10Z10W2Bias_Unorm));
        break;
    case ChNumFormat::X16_Unorm:
    case ChNumFormat::X16_Snorm:
    case ChNumFormat::X16_Uscaled:
    case ChNumFormat::X16_Sscaled:
    case ChNumFormat::X16_Uint:
    case ChNumFormat::X16_Sint:
    case ChNumFormat::X16_Float:
    case ChNumFormat::X16_MM10_Unorm:
    case ChNumFormat::X16_MM10_Uint:
    case ChNumFormat::X16_MM12_Unorm:
    case ChNumFormat::X16_MM12_Uint:
        isSame = ((dstFormat == ChNumFormat::X16_Unorm)     || (dstFormat == ChNumFormat::X16_Snorm)    ||
                  (dstFormat == ChNumFormat::X16_Uscaled)   || (dstFormat == ChNumFormat::X16_Sscaled)  ||
                  (dstFormat == ChNumFormat::X16_Uint)      || (dstFormat == ChNumFormat::X16_Sint)     ||
                  (dstFormat == ChNumFormat::X16_Float)     || (dstFormat == ChNumFormat::X16_MM10_Unorm) ||
                  (dstFormat == ChNumFormat::X16_MM10_Uint) || (dstFormat == ChNumFormat::X16_MM12_Unorm) ||
                  (dstFormat == ChNumFormat::X16_MM12_Uint));
        break;
    case ChNumFormat::X16Y16_Unorm:
    case ChNumFormat::X16Y16_Snorm:
    case ChNumFormat::X16Y16_Uscaled:
    case ChNumFormat::X16Y16_Sscaled:
    case ChNumFormat::X16Y16_Uint:
    case ChNumFormat::X16Y16_Sint:
    case ChNumFormat::X16Y16_Float:
    case ChNumFormat::X16Y16_MM10_Unorm:
    case ChNumFormat::X16Y16_MM10_Uint:
    case ChNumFormat::X16Y16_MM12_Unorm:
    case ChNumFormat::X16Y16_MM12_Uint:
        isSame = ((dstFormat == ChNumFormat::X16Y16_Unorm)     || (dstFormat == ChNumFormat::X16Y16_Snorm)    ||
                  (dstFormat == ChNumFormat::X16Y16_Uscaled)   || (dstFormat == ChNumFormat::X16Y16_Sscaled)  ||
                  (dstFormat == ChNumFormat::X16Y16_Uint)      || (dstFormat == ChNumFormat::X16Y16_Sint)     ||
                  (dstFormat == ChNumFormat::X16Y16_Float)     || (dstFormat == ChNumFormat::X16Y16_MM10_Unorm) ||
                  (dstFormat == ChNumFormat::X16Y16_MM10_Uint) || (dstFormat == ChNumFormat::X16Y16_MM12_Unorm) ||
                  (dstFormat == ChNumFormat::X16Y16_MM12_Uint));
        break;
    case ChNumFormat::X16Y16Z16W16_Unorm:
    case ChNumFormat::X16Y16Z16W16_Snorm:
    case ChNumFormat::X16Y16Z16W16_Uscaled:
    case ChNumFormat::X16Y16Z16W16_Sscaled:
    case ChNumFormat::X16Y16Z16W16_Uint:
    case ChNumFormat::X16Y16Z16W16_Sint:
    case ChNumFormat::X16Y16Z16W16_Float:
        isSame = ((dstFormat == ChNumFormat::X16Y16Z16W16_Unorm)   ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Snorm)   ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Uscaled) ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Sscaled) ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Uint)    ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Sint)    ||
                  (dstFormat == ChNumFormat::X16Y16Z16W16_Float));
        break;
    case ChNumFormat::X32_Uint:
    case ChNumFormat::X32_Sint:
    case ChNumFormat::X32_Float:
        isSame = ((dstFormat == ChNumFormat::X32_Uint) ||
                  (dstFormat == ChNumFormat::X32_Sint) ||
                  (dstFormat == ChNumFormat::X32_Float));
        break;
    case ChNumFormat::X32Y32_Uint:
    case ChNumFormat::X32Y32_Sint:
    case ChNumFormat::X32Y32_Float:
        isSame = ((dstFormat == ChNumFormat::X32Y32_Uint) ||
                  (dstFormat == ChNumFormat::X32Y32_Sint) ||
                  (dstFormat == ChNumFormat::X32Y32_Float));
        break;
    case ChNumFormat::X32Y32Z32_Uint:
    case ChNumFormat::X32Y32Z32_Sint:
    case ChNumFormat::X32Y32Z32_Float:
        isSame = ((dstFormat == ChNumFormat::X32Y32Z32_Uint) ||
                  (dstFormat == ChNumFormat::X32Y32Z32_Sint) ||
                  (dstFormat == ChNumFormat::X32Y32Z32_Float));
        break;
    case ChNumFormat::X32Y32Z32W32_Uint:
    case ChNumFormat::X32Y32Z32W32_Sint:
    case ChNumFormat::X32Y32Z32W32_Float:
        isSame = ((dstFormat == ChNumFormat::X32Y32Z32W32_Uint) ||
                  (dstFormat == ChNumFormat::X32Y32Z32W32_Sint) ||
                  (dstFormat == ChNumFormat::X32Y32Z32W32_Float));
        break;
    case ChNumFormat::Bc1_Unorm:
    case ChNumFormat::Bc1_Srgb:
        isSame = ((dstFormat == ChNumFormat::Bc1_Unorm) || (dstFormat == ChNumFormat::Bc1_Srgb));
        break;
    case ChNumFormat::Bc2_Unorm:
    case ChNumFormat::Bc2_Srgb:
        isSame = ((dstFormat == ChNumFormat::Bc2_Unorm) || (dstFormat == ChNumFormat::Bc2_Srgb));
        break;
    case ChNumFormat::Bc3_Unorm:
    case ChNumFormat::Bc3_Srgb:
        isSame = ((dstFormat == ChNumFormat::Bc3_Unorm) || (dstFormat == ChNumFormat::Bc3_Srgb));
        break;
    case ChNumFormat::Bc4_Unorm:
    case ChNumFormat::Bc4_Snorm:
        isSame = ((dstFormat == ChNumFormat::Bc4_Unorm) || (dstFormat == ChNumFormat::Bc4_Snorm));
        break;
    case ChNumFormat::Bc5_Unorm:
    case ChNumFormat::Bc5_Snorm:
        isSame = ((dstFormat == ChNumFormat::Bc5_Unorm) || (dstFormat == ChNumFormat::Bc5_Snorm));
        break;
    case ChNumFormat::Bc6_Ufloat:
    case ChNumFormat::Bc6_Sfloat:
        isSame = ((dstFormat == ChNumFormat::Bc6_Ufloat) || (dstFormat == ChNumFormat::Bc6_Sfloat));
        break;
    case ChNumFormat::Bc7_Unorm:
    case ChNumFormat::Bc7_Srgb:
        isSame = ((dstFormat == ChNumFormat::Bc7_Unorm) || (dstFormat == ChNumFormat::Bc7_Srgb));
        break;
    case ChNumFormat::Etc2X8Y8Z8_Unorm:
    case ChNumFormat::Etc2X8Y8Z8_Srgb:
        isSame = ((dstFormat == ChNumFormat::Etc2X8Y8Z8_Unorm) || (dstFormat == ChNumFormat::Etc2X8Y8Z8_Srgb));
        break;
    case ChNumFormat::Etc2X8Y8Z8W1_Unorm:
    case ChNumFormat::Etc2X8Y8Z8W1_Srgb:
        isSame = ((dstFormat == ChNumFormat::Etc2X8Y8Z8W1_Unorm) || (dstFormat == ChNumFormat::Etc2X8Y8Z8W1_Srgb));
        break;
    case ChNumFormat::Etc2X8Y8Z8W8_Unorm:
    case ChNumFormat::Etc2X8Y8Z8W8_Srgb:
        isSame = ((dstFormat == ChNumFormat::Etc2X8Y8Z8W8_Unorm) || (dstFormat == ChNumFormat::Etc2X8Y8Z8W8_Srgb));
        break;
    case ChNumFormat::Etc2X11_Unorm:
    case ChNumFormat::Etc2X11_Snorm:
        isSame = ((dstFormat == ChNumFormat::Etc2X11_Unorm) || (dstFormat == ChNumFormat::Etc2X11_Snorm));
        break;
    case ChNumFormat::Etc2X11Y11_Unorm:
    case ChNumFormat::Etc2X11Y11_Snorm:
        isSame = ((dstFormat == ChNumFormat::Etc2X11Y11_Unorm) || (dstFormat == ChNumFormat::Etc2X11Y11_Snorm));
        break;
    case ChNumFormat::AstcLdr4x4_Unorm:
    case ChNumFormat::AstcLdr4x4_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr4x4_Unorm) || (dstFormat == ChNumFormat::AstcLdr4x4_Srgb));
        break;
    case ChNumFormat::AstcLdr5x4_Unorm:
    case ChNumFormat::AstcLdr5x4_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr5x4_Unorm) || (dstFormat == ChNumFormat::AstcLdr5x4_Srgb));
        break;
    case ChNumFormat::AstcLdr5x5_Unorm:
    case ChNumFormat::AstcLdr5x5_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr5x5_Unorm) || (dstFormat == ChNumFormat::AstcLdr5x5_Srgb));
        break;
    case ChNumFormat::AstcLdr6x5_Unorm:
    case ChNumFormat::AstcLdr6x5_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr6x5_Unorm) || (dstFormat == ChNumFormat::AstcLdr6x5_Srgb));
        break;
    case ChNumFormat::AstcLdr6x6_Unorm:
    case ChNumFormat::AstcLdr6x6_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr6x6_Unorm) || (dstFormat == ChNumFormat::AstcLdr6x6_Srgb));
        break;
    case ChNumFormat::AstcLdr8x5_Unorm:
    case ChNumFormat::AstcLdr8x5_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr8x5_Unorm) || (dstFormat == ChNumFormat::AstcLdr8x5_Srgb));
        break;
    case ChNumFormat::AstcLdr8x6_Unorm:
    case ChNumFormat::AstcLdr8x6_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr8x6_Unorm) || (dstFormat == ChNumFormat::AstcLdr8x6_Srgb));
        break;
    case ChNumFormat::AstcLdr8x8_Unorm:
    case ChNumFormat::AstcLdr8x8_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr8x8_Unorm) || (dstFormat == ChNumFormat::AstcLdr8x8_Srgb));
        break;
    case ChNumFormat::AstcLdr10x5_Unorm:
    case ChNumFormat::AstcLdr10x5_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr10x5_Unorm) || (dstFormat == ChNumFormat::AstcLdr10x5_Srgb));
        break;
    case ChNumFormat::AstcLdr10x6_Unorm:
    case ChNumFormat::AstcLdr10x6_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr10x6_Unorm) || (dstFormat == ChNumFormat::AstcLdr10x6_Srgb));
        break;
    case ChNumFormat::AstcLdr10x8_Unorm:
    case ChNumFormat::AstcLdr10x8_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr10x8_Unorm) || (dstFormat == ChNumFormat::AstcLdr10x8_Srgb));
        break;
    case ChNumFormat::AstcLdr10x10_Unorm:
    case ChNumFormat::AstcLdr10x10_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr10x10_Unorm) || (dstFormat == ChNumFormat::AstcLdr10x10_Srgb));
        break;
    case ChNumFormat::AstcLdr12x10_Unorm:
    case ChNumFormat::AstcLdr12x10_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr12x10_Unorm) || (dstFormat == ChNumFormat::AstcLdr12x10_Srgb));
        break;
    case ChNumFormat::AstcLdr12x12_Unorm:
    case ChNumFormat::AstcLdr12x12_Srgb:
        isSame = ((dstFormat == ChNumFormat::AstcLdr12x12_Unorm) || (dstFormat == ChNumFormat::AstcLdr12x12_Srgb));
        break;
    case ChNumFormat::X8Y8_Z8Y8_Unorm:
    case ChNumFormat::X8Y8_Z8Y8_Uscaled:
        isSame = ((dstFormat == ChNumFormat::X8Y8_Z8Y8_Unorm) || (dstFormat == ChNumFormat::X8Y8_Z8Y8_Uscaled));
        break;
    case ChNumFormat::Y8X8_Y8Z8_Unorm:
    case ChNumFormat::Y8X8_Y8Z8_Uscaled:
        isSame = ((dstFormat == ChNumFormat::Y8X8_Y8Z8_Unorm) || (dstFormat == ChNumFormat::Y8X8_Y8Z8_Uscaled));
        break;
    case ChNumFormat::Count:
        isSame = false;
        break;
    }

    return isSame;
}

// =====================================================================================================================
// Determines whether the format is an MM format
bool IsMmFormat(
    ChNumFormat format)
{
    return (((format >= ChNumFormat::X8_MM_Unorm) &&
             (format <= ChNumFormat::X16Y16_MM10_Uint)) ||
            ((format >= ChNumFormat::X16_MM12_Unorm) &&
             (format <= ChNumFormat::X16Y16_MM12_Uint)));
}

} // Formats
} // Pal
