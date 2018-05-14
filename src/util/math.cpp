/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palMath.h"
#include <cmath>

namespace Util
{
namespace Math
{

// Properties of an N-bit floating point number.
struct NBitFloatInfo
{
    uint32 numBits;       // Total number of bits.
    uint32 numFracBits;   // Number of fractional (mantissa) bits.
    uint32 numExpBits;    // Number of (biased) exponent bits.
    uint32 signBit;       // Position of the sign bit, zero if floatN is unsigned.
    uint32 signMask;      // Mask to extract sign bit, zero if floatN is unsigned.
    uint32 fracMask;      // Mask to extract mantissa bits.
    uint32 expMask;       // Mask to extract exponent bits.
    int32  expBias;       // Bias for the exponent.
    uint32 eMax;          // Maximum value for the exponent.
    int32  eMin;          // Minimum value for the exponent.
    uint32 maxNormal;     // Max value which can be represented by an N-bit float.
    uint32 minNormal;     // Min value which can be represented by an N-bit float.
    uint32 biasDiff;      // Difference in bias between floatN and float32 exponents.
    uint32 fracBitsDiff;  // Difference in number of mantissa bits between floatN and float32.
};

// Static function declarations.
static uint32 Float32ToFloatN(float f, const NBitFloatInfo& info);
static float FloatNToFloat32(uint32 fBits, const NBitFloatInfo& info);

// Initialize the descriptors for various N-bit floating point representations:
static constexpr NBitFloatInfo Float16Info =
{
    16,                                                       // numBits
    10,                                                       // numFracBits
    5,                                                        // numExpBits
    15,                                                       // signBit
    (1 << 15),                                                // signMask
    (1 << 10) - 1,                                            // fracMask
    ((1 << 5) - 1) << 10,                                     // expMask
    (1 << (5 - 1)) - 1,                                       // expBias
    (1 << (5 - 1)) - 1,                                       // eMax
    -((1 << (5 - 1)) - 1) + 1,                                // eMin
    ((((1 << (5 - 1)) - 1) + 127) << 23) | 0x7FE000,          // maxNormal
    ((-((1 << (5 - 1)) - 1) + 1) + 127) << 23,                // minNormal
    (((1U << (5 - 1)) - 1) - 127) << 23,                      // biasDiff
    (23 - 10),                                                // fracBitsDiff
};

static constexpr NBitFloatInfo Float11Info =
{
    11,                                                       // numBits
    6,                                                        // numFracBits
    5,                                                        // numExpBits
    0,                                                        // signBit
    0,                                                        // signMask
    (1 << 6) - 1,                                             // fracMask
    ((1 << 5) - 1) << 6,                                      // expMask
    (1 << (5 - 1)) - 1,                                       // expBias
    (1 << (5 - 1)) - 1,                                       // eMax
    -((1 << (5 - 1)) - 1)+1,                                  // eMin
    ((((1 << (5 - 1)) - 1) + 127) << 23) | 0x7E000,           // maxNormal
    ((-((1 << (5 - 1)) - 1) + 1) + 127) << 23,                // minNormal
    (((1U << (5 - 1)) - 1) - 127) << 23,                      // biasDiff
    23 - 6,                                                   // fracBitsDiff
};

static constexpr NBitFloatInfo Float10Info =
{
    10,                                                       // numBits
    5,                                                        // numFracBits
    5,                                                        // numExpBits
    0,                                                        // signBit
    0,                                                        // signMask
    (1 << 5) - 1,                                             // fracMask
    ((1 << 5) - 1) << 5,                                      // expMask
    (1 << (5 - 1)) - 1,                                       // expBias
    (1 << (5 - 1)) - 1,                                       // eMax
    -((1 << (5 - 1)) - 1)+1,                                  // eMin
    ((((1 << (5 - 1)) - 1) + 127) << 23) | 0x7C000,           // maxNormal
    ((-((1 << (5 - 1)) - 1) + 1) + 127) << 23,                // minNormal
    (((1U << (5 - 1)) - 1) - 127) << 23,                      // biasDiff
    23 - 5,                                                   // fracBitsDiff
};

// =====================================================================================================================
// Checks if a nunmber is denormalized.
bool IsDenorm(
    float f)
{
    const uint32 bits = (FloatToBits(f) & FloatMaskOutSignBit);

    return (bits < MinNormalizedFloatBits);
}

// =====================================================================================================================
// Checks if a nunmber is +/- infinity.
bool IsInf(
    float f)
{
    const uint32 bits = FloatToBits(f);

    bool isInf = false;

    if (((bits & FloatExponentMask) == FloatExponentMask) && ((bits & FloatMantissaMask ) == 0))
    {
        isInf = true;
    }

    return isInf;
}

// =====================================================================================================================
// Checks if a nunmber is QNaN or SNaN.
bool IsNaN(
    float f)
{
    const uint32 bits = FloatToBits(f);

    bool isNaN = false;

    if (((bits & FloatExponentMask) == FloatExponentMask) && ((bits & FloatMantissaMask ) != 0))
    {
        isNaN = true;
    }

    return isNaN;
}

// =====================================================================================================================
// Converts a floating point number to an unsigned fixed point number with the given integer and fractional bits.
uint32 FloatToUFixed(
    float  f,
    uint32 intBits,
    uint32 fracBits,
    bool   enableRounding)
{
    uint32 fixedPtNum;
    uint32 clampVal;
    float  floatVal;

    // Since we're handling both.
    PAL_ASSERT(intBits <= 32);

    // Cannot handle more than 32 bits.
    PAL_ASSERT((intBits + fracBits) <= 32);

    if (intBits == 32)
    {
        // Full 32 bit unsigned integer.  fracBits must be zero.
        PAL_ASSERT(fracBits == 0);

        // Make sure the unsigned integer is not negative.
        floatVal = Max(f, FloatZero);
        clampVal = 0xFFFFFFFF;
    }
    else
    {
        uint32 scale;
        float  maxVal;

        // If we don't have any actual integer bits for an signed number, 1.0 should be represented as the max
        // fractional value.  E.g. for 8 fractional bits 1.0 should be 255. Otherwise, you can never represent +/-1.0.
        // The scale value is adjusted appropriately below.
        if (intBits == 0)
        {
            scale    = (0x1 << fracBits) - 1;
            maxVal   = 1.0;
            clampVal = scale;
        }
        else
        {
            scale    =  (0x1 << fracBits);

            // Largest intBits.fracBits positive number = 2^(intBits) - (1/(2^fracBits)).
            maxVal   = static_cast<float>(0x1 << (intBits)) - (FloatOne / static_cast<float>((0x1 << (fracBits))));
            clampVal = static_cast<uint32>(scale * maxVal);
        }

        // Clamp to min/max.
        floatVal = Clamp(f, FloatZero, maxVal);

        // Convert to integer scale.
        floatVal = floatVal * scale;
    }

    // Round before conversion if enabled.
    if (enableRounding)
    {
        if (floatVal > 0)
        {
            floatVal += 0.5f;
        }
        else
        {
            floatVal -= 0.5f;
        }
    }

    // Due to rounding, the float val may overflow.
    if (IsNaN(f))
    {
        fixedPtNum = 0;
    }
    else if (floatVal >= clampVal)
    {
        fixedPtNum = clampVal;
    }
    else
    {
        // Convert to fixed point.
        fixedPtNum = static_cast<uint32>(floatVal);
    }

    return fixedPtNum;
}

// =====================================================================================================================
// Converts a floating point number to a signed fixed point number with the given integer and fractional bits.
uint32 FloatToSFixed(
    float  f,
    uint32 intBits,
    uint32 fracBits,
    bool   enableRounding)
{
    uint32 fixedPtNum;
    uint32 scale;
    uint32 clampPos;
    int32  clampNeg;
    float  floatVal;

    // Cannot handle more than 32 bits.
    PAL_ASSERT(intBits <= 32);
    PAL_ASSERT((intBits + fracBits) <= 32);

    if (intBits == 32)
    {
        // Full 32 bit signed integer. numFracBits must be zero.
        PAL_ASSERT(fracBits == 0);

        floatVal = f;
        clampPos = 0x7FFFFFFF;
        clampNeg = 0x80000000;
    }
    else
    {
        float maxVal;
        float minVal;

        if (intBits == 0)
        {
            // Sorry, can't have a 0.0 number.
            PAL_ASSERT(fracBits != 0);

            // If we don't have any actual integer bits for an signed number, 1.0 should be represented asthe max
            // fractional value.  E.g. for 8 fractional bits 1.0 should be 255. Otherwise, you can never represent
            // +/-1.0. The scale value is adjusted below to take this into account.

            // fracBits includes a bit for the sign, so the actual available bits is one less.
            scale    = (0x1 << (fracBits-1)) - 1;

            minVal   = FloatNegOne;
            maxVal   = FloatOne;
            clampPos = scale;
            clampNeg = -static_cast<int32>(scale);
        }
        else
        {
            scale    = (0x1 << fracBits);

            // intBits includes a bit for the sign, so the actual available bits is one less.  Smallest intBits.fracBits
            // negative number = -2^(intBits-1)
            minVal   = static_cast<float>(-(0x1 << (intBits-1)));

            // Largest intBits.fracBits positive number = 2^(intBits-1) - (1/(2^fracBits)).
            maxVal   = static_cast<float>(0x1 << (intBits-1)) -
                       (FloatOne / static_cast<float>((0x1 << (fracBits))));
            clampPos = static_cast<uint32>(scale * maxVal);
            clampNeg = static_cast<int32>(scale * minVal);
        }

        // Clamp to min/max.
        floatVal = Clamp(f, minVal, maxVal);

        // Convert to integer scale.
        floatVal = floatVal * scale;
    }

    // Round before conversion if enabled.
    if (enableRounding)
    {
        if (floatVal > 0)
        {
            floatVal += 0.5f;
        }
        else
        {
            floatVal -= 0.5f;
        }
    }

    // Due to rounding, the float val may overflow.
    if (IsNaN(f))
    {
        fixedPtNum = 0;
    }
    else if (floatVal >= clampPos)
    {
        fixedPtNum = clampPos;
    }
    else if (floatVal <= clampNeg)
    {
        fixedPtNum = clampNeg;
    }
    else
    {
        // Convert to fixed point.
        fixedPtNum = static_cast<int32>(floatVal);
    }

    return fixedPtNum;
}

// =====================================================================================================================
// Converts a signed fixed point number with the given integer and fractional bits to a floating point number.
float SFixedToFloat(
    int32  fixedPtNum,
    uint32 intBits,
    uint32 fracBits)
{
    PAL_ASSERT((fracBits + intBits) <= 32);

    // We static_cast a signed fixed-point number contained in the input int32 to float below in multiple places, but
    // this fixed-point number might not span the full 32 bits.  In order for the static_cast to correctly convert the
    // signed value to floating point, we need to sign-extend fixedPtNum to full 32 bits.
    const uint32 outsideBitCount = 32 - (intBits + fracBits);

    fixedPtNum <<= outsideBitCount;
    fixedPtNum >>= outsideBitCount;

    float result;

    // If we don't have any actual integer bits for a signed number, 1.0 should be represented as the max fractional
    // value.  E.g. for 8 fractional bits 1.0 should be 255.  Otherwise, you can never represent +/-1.0. The factor is
    // adjusted appropriately below.
    if (intBits == 0)
    {
        // If the number of integer bits is zero, then the sign is part of the fractional bits, and we must subtract 1
        // from the numFracBits to get the factor.
        const uint32 factor = (1 << (fracBits - 1)) - 1;
        result = static_cast<float>(fixedPtNum) / factor;
    }
    else
    {
        // If the number of integer bits is not zero, then the sign is part of the integer bits, and the conversion to
        // float take the sign into account.  If fracBits is zero, again the cast to FLOAT will result in the correct
        // sign If fracBits is non-zero the fraction will remain positive.

        // For the most common usage of this function (from the format conversion routines) the fracBits and numIntBits
        // are known at compile time, so the compiler will get rid of the if condition.
        if (fracBits == 0)
        {
            result = static_cast<float>(fixedPtNum);
        }
        else
        {
            // Assert if the fixed point representation cannot be converted to a float32.
            PAL_ASSERT(fracBits <= 23);
            PAL_ASSERT((fracBits + intBits) <= 24);

            const float intVal          = static_cast<float>(fixedPtNum >> fracBits);
            const uint32  factor        = (1 << fracBits);
            const uint32  numeratorBits = (fixedPtNum & ((1 << fracBits) - 1));
            const float fracVal         = (static_cast<float>(numeratorBits) / factor);

            result = (intVal + fracVal);
        }
    }

    return result;
}

// =====================================================================================================================
// Converts an unsigned fixed point number with the given integer and fractional bits to a floating point number.
float UFixedToFloat(
    uint32 fixedPtNum,
    uint32 intBits,
    uint32 fracBits)
{
    PAL_ASSERT((fracBits <= 32) && (intBits <= 32));

    float result;

    // If we don't have any actual integer bits for an unsigned number, 1.0 should be represented as the max fractional
    // value.  E.g. for 8 fractional bits 1.0 should be 255.  Otherwise, you can never represent 1.0. The factor
    // value is adjusted appropriately below.
    if (intBits == 0)
    {
        const uint32 factor = (1 << fracBits) - 1;
        result = (static_cast<float>(fixedPtNum) / factor);
    }
    else
    {
        // Soft assert here if the fixed point representation cannot be converted to a float32. This will cause precision
        // loss
        PAL_ALERT(fracBits > 23);
        PAL_ALERT((fracBits + intBits) > 24);

        // Calculate the scaling factor.
        const uint32 factor = (1 << fracBits);

        // Directly convert the unsigned fixed point number to float.
        const float intVal = static_cast<float>(fixedPtNum);

        // Perform scaling.
        result = intVal / factor;
    }

    return result;
}

// =====================================================================================================================
// Converts a 32-bit IEEE floating-point number to an N-bit signed or unsigned floating point representation.
static uint32 Float32ToFloatN(
    float                f,     // 32-bit floating point number to convert.
    const NBitFloatInfo& info)  // Descriptor for the N-bit floating point number.
{
    static_assert(sizeof(uint32) == sizeof(float), "uint32 and float are not the same size!");

    const uint32 fBits    = FloatToBits(f);
    uint32       fAbsBits = (fBits & FloatMaskOutSignBit);

    // Only extract sign bit if the desired representation will have one.
    uint32 sign = 0;
    if (info.signMask != 0)
    {
        // If the sign mask is nonzero, the destination float is assumed to have a sign bit.
        sign = (fBits & FloatSignBitMask) >> (info.numFracBits + info.numExpBits + 1);
    }

    uint32 floatN;

    if (IsNaN(f))
    {
        // Create an N-bit NaN value.
        floatN = (info.expMask | info.fracMask);
    }
    else if ((info.signMask == 0) && (TestAnyFlagSet(fBits, FloatSignBitMask) == true))
    {
        // If the input is negative and our output format is unsigned, clamp to zero.
        floatN = 0;
    }
    else if (IsInf(f))
    {
        // Create an N-bit +/- Inf value.
        floatN = (sign | info.expMask);
    }
    else if (fAbsBits > info.maxNormal)
    {
        // Input is not representable using an N-bit float --> clamp to floatN_max.
        floatN = static_cast<uint32>(sign | ((((1 << info.numExpBits) - 2)) << info.numFracBits) | info.fracMask);
    }
    else if (fAbsBits < info.minNormal)
    {
        // Denormalized input value - make implicit 1 explicit.
        const uint32 fracBits = (fAbsBits & FloatMantissaMask) | (1 << FloatNumMantissaBits);
        const int32  nShift   = ((info.eMin + FloatExponentBias) - (fAbsBits >> FloatNumMantissaBits));

        fAbsBits = (nShift < 24) ? (fracBits >> nShift) : 0;
        // Round to zero.
        floatN = static_cast<uint32>(sign | (fAbsBits >> info.fracBitsDiff));
    }
    else
    {
        // Normalize value and round to zero.
        floatN = static_cast<uint32>(sign | ((fAbsBits + info.biasDiff) >> info.fracBitsDiff));
    }

    return floatN;
}

// =====================================================================================================================
// Converts a 32-bit IEEE floating-point number to a 16-bit signed floating-point number.
uint32 Float32ToFloat16(
    float f)
{
    return Float32ToFloatN(f, Float16Info);
}

// =====================================================================================================================
// Converts a 32-bit IEEE floating-point number to an 11-bit unsigned floating-point number.
uint32 Float32ToFloat11(
    float f)
{
    return Float32ToFloatN(f, Float11Info);
}

// =====================================================================================================================
// Converts a 32-bit IEEE floating-point number to a 10-bit unsigned floating-point number.
uint32 Float32ToFloat10(
    float f)
{
    return Float32ToFloatN(f, Float10Info);
}

// =====================================================================================================================
// Converts an N-bit signed or unsigned floating-point number to a 32-bit IEEE floating point representation.  Does not
// fully handle denormalized inputs.
static float FloatNToFloat32(
    uint32               fBits,  // N-bit floating point number packed into a uint32.
    const NBitFloatInfo& info)   // Descriptor for the N-bit floating point number.
{
    PAL_ASSERT((info.numFracBits + info.numExpBits) <= 31);

    uint32 fAbsBits = 0;
    if (info.signBit)
    {
        // MSB is the sign bit.
        fAbsBits = fBits & ((1 << (info.signBit + 1)) - 1);
    }
    else
    {
        // No sign bit present.
        fAbsBits = fBits & ((1 << (info.numFracBits + info.numExpBits)) - 1);
    }

    // Extract the exponent.
    uint32 exp = (fAbsBits & info.expMask);

    uint32 resultBits = 0;
    if (exp == 0)
    {
        if ((fAbsBits & info.fracMask) != 0)
        {
            // Normalizing the denormalized value.
            uint32 fracBits = fAbsBits & info.fracMask;
            exp             = info.eMin;

            while ((fracBits & (info.fracMask + 1)) == 0)
            {
                --exp;
                fracBits >>= 1;
            }

            fracBits  &= ~(info.fracMask + 1);  // Remove hidden bit

            resultBits = ((fAbsBits & info.signMask) << info.numBits)        | // Sign bit
                         ((exp + FloatExponentBias) << FloatNumMantissaBits) | // Exponent
                         (fracBits << info.fracBitsDiff);                      // Mantissa
        }
        else
        {
            // Zero-only sign bit is used for unsigned floats, this results in zero because info.signMask should be
            // zero here.
            resultBits = (fAbsBits & info.signMask) << info.numBits;
        }
    }
    else if (exp == info.expMask)
    {
        resultBits = ((fAbsBits & info.signMask) << info.numBits)     |  // Sign bit
                     FloatExponentMask                                |  // Exponent
                     ((fAbsBits & info.fracMask) << info.fracBitsDiff);  // Mantissa
    }
    else
    {
        const uint32 exponent =
            (((fAbsBits >> info.numFracBits) & ((1 << info.numExpBits) - 1))
                - info.expBias + FloatExponentBias);

        resultBits = ((fAbsBits & info.signMask) << info.numBits)     |  // Sign bit
                     (exponent << FloatNumMantissaBits)               |  // Exponent
                     ((fAbsBits & info.fracMask) << info.fracBitsDiff);  // Mantissa
    }

    // Interpret the result bits as a floating-point number.
    float result;
    (*reinterpret_cast<uint32*>(&result)) = resultBits;
    return result;
}

// =====================================================================================================================
// Converts a 16-bit signed floating-point number to a 32-bit IEEE floating point number.
float Float16ToFloat32(
    uint32 fBits)
{
    return FloatNToFloat32(fBits, Float16Info);
}

// =====================================================================================================================
// Converts an 11-bit unsigned floating-point number to a 32-bit IEEE floating point number.
float Float11ToFloat32(
    uint32 fBits)
{
    return FloatNToFloat32(fBits, Float11Info);
}

// =====================================================================================================================
// Converts an 10-bit unsigned floating-point number to a 32-bit IEEE floating point number.
float Float10ToFloat32(
    uint32 fBits)
{
    return FloatNToFloat32(fBits, Float10Info);
}

// =====================================================================================================================
// Computes the square root of the given input number.  This is a trivially simple function, since we delegate to the
// standard C-runtime sqrtf() function.
float Sqrt(
    float number)
{
    PAL_ASSERT(number >= 0.0f); // Let's avoid imaginary numbers, please.
    return sqrtf(number);
}

// =====================================================================================================================
// Computes the power function on the given base and exponent. This is a trivially simple function, since we delegate to
// the standard C-runtime powf() function.
float Pow(
    float base,
    float exponent)
{
    return powf(base, exponent);
}

// =====================================================================================================================
// Computes the absolute value. This is a trivially simple function, since we delegate to
// the standard C-runtime abs() function.
uint32 Absu(
    int32 number)
{
    return static_cast<uint32>(abs(number));
}

// =====================================================================================================================
// Converts the input 32-bit floating point number to a uint32 which stores the IEEE representation of the float in the
// specified number of bits.
uint32 Float32ToNumBits(
    float  float32,
    uint32 numBits)
{
    uint32  retVal = 0;

    // We should only expect to see floating point numbers of either 32, 16, 11 or 10 bits wide.
    if (numBits == 32)
    {
        retVal = FloatToBits(float32);
    }
    else if (numBits == 16)
    {
        retVal = Float32ToFloat16(float32);
    }
    else if (numBits == 11)
    {
        retVal = Float32ToFloat11(float32);
    }
    else if (numBits == 10)
    {
        retVal = Float32ToFloat10(float32);
    }
    else
    {
        PAL_NEVER_CALLED();
    }

    return retVal;
}

// =====================================================================================================================
// Converts the input "numBits" width IEEE floating point number to a float.
float FloatNumBitsToFloat32(
    uint32  input,
    uint32  numBits)
{
    float  float32 = 0;

    switch (numBits)
    {
    case 32:
        // "input" is the integer / IEEE representation of a float, so just typecast.
        float32 = *(reinterpret_cast<float*>(&input));
        break;

    case 16:
        float32 = Float16ToFloat32(input);
        break;

    case 11:
        float32 = Float11ToFloat32(input);
        break;

    case 10:
        float32 = Float10ToFloat32(input);
        break;

    default:
        PAL_ASSERT_ALWAYS();
        break;
    }

    return float32;
}

// =====================================================================================================================
// Converts the input 32-bit floating point number to a fraction which stores the value in a fractional form.
Fraction Float32ToFraction(
    float float32)
{
    Fraction frac = {};

    const uint32 floatBits = FloatToBits(float32);

    // We'll derive the nominator from the mantissa and the denominator from the exponent
    uint32 mantissa = (1 << FloatNumMantissaBits) | (floatBits & FloatMantissaMask);
    int32  exponent = ((floatBits & FloatExponentMask) >> FloatNumMantissaBits) - FloatExponentBias
                      - FloatNumMantissaBits; // Also subtract mantissa bits as we treat the mantissa as integer

    // Apply positive exponent to the mantissa
    if (exponent > 0)
    {
        mantissa <<= exponent;
        exponent = 0;
    }

    // Remove trailing zeros
    uint32 index = 0;
    Util::BitMaskScanForward(&index, mantissa);
    index = Util::Min(index, static_cast<uint32>(-exponent));
    mantissa >>= index;
    exponent += index;

    frac.num = mantissa;
    frac.den = 1 << (-exponent);

    return frac;
}

// =====================================================================================================================
// Convert the input to sign-preserved zero if input is denorm, otherwise return input value
float FlushDenormToZero(
    float input)
{
    if (IsDenorm(input) == true)
    {
        uint32 b = FloatToBits(input);
        b = FloatSignBitMask & b;
        SetBitsToFloat(&input, b);
    }
    return input;
}

// =====================================================================================================================
// Converts a signed 8 bit number into a 1.7 signed magnitude scheme. Valid input range is (-127, 127)
uint8 IntToSignedMagnitude(
    int8 input)
{
    uint8 output      = 0;
    uint8 absoluteVal = static_cast<uint8>(Math::Absu(input));

    // Only numbers from (-127, 127) can be represented in this scheme.
    PAL_ASSERT(absoluteVal < 128);

    if (input < 0)
    {
        // Sign bit
        output = (0x1 << 7);
    }

    return (output | ( 0x7f & absoluteVal ));
}

} // Math
} // Util
