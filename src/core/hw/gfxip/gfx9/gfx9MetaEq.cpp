/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"
#include "palInlineFuncs.h"
#include "palIterator.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9Image.h"
#include "core/hw/gfxip/gfx9/gfx9MetaEq.h"
#include "g_gfx9Settings.h"

using namespace Util;

namespace Pal
{
namespace Gfx9
{

//=============== Implementation for MetaDataAddrEquation: =============================================================
// =====================================================================================================================
MetaDataAddrEquation::MetaDataAddrEquation(
    uint32         maxEquationBits, // maximum number of bits this equation could possibly have
    const char*    pName)           // a identifier for this equation, only used for debug prints.  Can be NULL
    :
    m_maxBits(maxEquationBits)
{
    PAL_ASSERT (maxEquationBits < MaxNumMetaDataAddrBits);

    memset(&m_firstPair[0], 0, sizeof(m_firstPair));

#if PAL_ENABLE_PRINTS_ASSERTS
    Strncpy(m_equationName, ((pName != nullptr) ? pName : ""), MaxEquationNameLength);
#endif

    Reset();
}

// =====================================================================================================================
void MetaDataAddrEquation::ClearBitPos(
    uint32  bitPos)
{
    for (uint32 compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
    {
        ClearBits(bitPos, compType, 0);
    }
}

// =====================================================================================================================
void MetaDataAddrEquation::ClearBits(
    uint32   bitPos,     // the bit position of the equation to look at
    uint32   compType,   // one of MetaDataAddrCompType enumerations
    uint32   keepMask)   // set bits in the mask are *kept*
{
    ValidateInput(bitPos, compType);

    m_equation[bitPos][compType] &= keepMask;
}

// =====================================================================================================================
// Returns the result of "pair0 compareType pair1"
bool MetaDataAddrEquation::CompareCompPair(
    const CompPair&           pair0,
    const CompPair&           pair1,
    MetaDataAddrCompareTypes  compareType)
{
    const int8  s0CompPos = static_cast<int8>(pair0.compPos);
    const int8  s1CompPos = static_cast<int8>(pair1.compPos);

    bool  bRetVal = false;

    switch (compareType)
    {
    case MetaDataAddrCompareLt:
        // SEE:  COORD::operator<
        if (pair0.compType == pair1.compType)
        {
            bRetVal = (s0CompPos < s1CompPos);
        }
        else if ((pair0.compType == MetaDataAddrCompS) || (pair1.compType == MetaDataAddrCompM))
        {
            bRetVal = true;
        }
        else if ((pair1.compType == MetaDataAddrCompS) || (pair0.compType == MetaDataAddrCompM))
        {
            bRetVal = false;
        }
        else if (pair0.compPos == pair1.compPos)
        {
            bRetVal = (pair0.compType < pair1.compType);
        }
        else
        {
            bRetVal = (s0CompPos < s1CompPos);
        }
        break;

    case MetaDataAddrCompareEq:
        // SEE:  COORD::operator==
        bRetVal = ((pair0.compType == pair1.compType) && (pair0.compPos  == pair1.compPos));
        break;

    case MetaDataAddrCompareGt:
        // SEE:  COORD::operator>
        bRetVal = ((CompareCompPair(pair0, pair1, MetaDataAddrCompareLt) == false) &&
                   (CompareCompPair(pair0, pair1, MetaDataAddrCompareEq) == false));
        break;

    default:
        PAL_NOT_IMPLEMENTED();
        break;
    }

    return bRetVal;
}

// =====================================================================================================================
void MetaDataAddrEquation::Copy(
    MetaDataAddrEquation*  pDst,
    uint32                 startBitPos,
    int32                  copySize
    ) const
{
    const uint32  numBitsToCopy = (copySize == -1) ? m_maxBits : copySize;
    pDst->SetEquationSize(numBitsToCopy);

    for(uint32 bitPosIndex = 0; bitPosIndex < numBitsToCopy; bitPosIndex++)
    {
        for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
        {
            pDst->ClearBits(bitPosIndex, compType, 0);
            pDst->SetMask(bitPosIndex, compType, Get(startBitPos + bitPosIndex, compType));
        }
    }
}

// =====================================================================================================================
// Uses the CPU to solve the meta-equation given the specified inputs.  The return value is always in terms of nibbles
uint32 MetaDataAddrEquation::CpuSolve(
    uint32  x,         // cartesian coordinates
    uint32  y,
    uint32  z,         // which slice of either a 2d array or 3d volume
    uint32  sample,    // which msaa sample
    uint32  metaBlock  // which metablock
    ) const
{
    uint32  metaOffset = 0;

    for (uint32  bitPos = 0; bitPos < GetNumValidBits(); bitPos++)
    {
        uint32 b =
             (CountSetBits(Get(bitPos, MetaDataAddrCompX) & x)         & 0x1);
        b ^= (CountSetBits(Get(bitPos, MetaDataAddrCompY) & y)         & 0x1);
        b ^= (CountSetBits(Get(bitPos, MetaDataAddrCompZ) & z)         & 0x1);
        b ^= (CountSetBits(Get(bitPos, MetaDataAddrCompS) & sample)    & 0x1);
        b ^= (CountSetBits(Get(bitPos, MetaDataAddrCompM) & metaBlock) & 0x1);

        metaOffset |= (b << bitPos);
    } // end loop through all the bits in the equation

    return metaOffset;
}

// =====================================================================================================================
// Returns true if the specified compType / data pair appears anywhere in this equation.  Otherwise, this returns
// false
bool MetaDataAddrEquation::Exists(
    uint32  compType,
    uint32  inputMask
    ) const
{
    bool    allBitsFound = true;
    uint32  lowPos       = 0;

    // "inputMask" might have multiple bits set in it (i.e., x3 ^ x5); we have to look for every one.  Extract each
    // set bit in "inputMask"
    while ((allBitsFound == true) && (BitMaskScanForward(&lowPos, inputMask) == true))
    {
        const uint32  lowPosMask = 1 << lowPos;
        bool          localFound = false;

        // Look through all the bits in this equation for the "lowPos" bit.  Once (if...) it's found, just quit.
        for (uint32  eqBitPos = 0; ((localFound == false) && (eqBitPos < m_maxBits)); eqBitPos++)
        {
            const uint32  eqData = Get(eqBitPos, compType);
            localFound = TestAnyFlagSet(eqData, lowPosMask);
        }

        // We need to find all the bits, so stop at the first non-found bit from "inputMask".
        allBitsFound &= localFound;

        // And remove the just searched-for bit from the inputMask so that we don't look for it again.
        inputMask &= ~lowPosMask;
    }

    return allBitsFound;
}

// =====================================================================================================================
//
// Essentially, this function is comparing the data at the equations's "eqBitPos / compType" with "compPair", using
// "compareFunc" and eliminating any bits that fail the test.
void MetaDataAddrEquation::FilterOneCompType(
    MetaDataAddrCompareTypes   compareFunc,
    const CompPair&            compPair,
    uint32                     eqBitPos,     // bit of the equation we're interested in
    MetaDataAddrComponentType  compType,     // the component type we're interested in
    MetaDataAddrComponentType  axis)
{
    if ((axis == MetaDataAddrCompNumTypes) || (axis == compType))
    {
        uint32  eqData     = Get(eqBitPos, compType);
        for (uint32 dataBitPos : BitIter32(eqData))
        {
            const CompPair eqCompPair  = SetCompPair(compType, dataBitPos);
            const uint32   dataBitMask = ~(1 << dataBitPos);

            if (CompareCompPair(eqCompPair, compPair, compareFunc))
            {
                ClearBits(eqBitPos, compType, dataBitMask);
            }
        }
    } // end check for anything to do
}

// =====================================================================================================================
// Filter looks at the equation and removes anything from the equation that passes the comparison test.
//    i.e., if the equation was:
//               eq[0] = x4 ^ y3
//               eq[1] = x7 ^ y7 ^ z3
//
//  and "compPair = x5" and "compareFunc" was "<"
//
//  then we 'd be left with:
//               eq[0] = y3
//               eq[1] = x7 ^ y7 ^ z3
//
//  Another pass with "compPair = y3" and "compareFunc" being "==" would produce:
//               eq[0] = x7 ^ y7 ^ z3
//
void MetaDataAddrEquation::Filter(
    const CompPair&            compPair,
    MetaDataAddrCompareTypes   compareFunc,
    uint32                     startBit,
    MetaDataAddrComponentType  axis)
{
    uint32  bitPos = startBit;
    while (bitPos < GetNumValidBits())
    {
        // This loop is the equivalent of:
        //   m = eq[i].Filter( f, co, 0, axis );
        //
        // where:
        //    'f'     is compareFunc
        //    'co'    is compPair
        //    'axis'  is axis
        //    'eq[i]' is a single bit of the equation.  i.e., x5 ^ x3 ^ y3 ^ z4.  We have to filter the components
        //            one at a time.
        //
        //    'm' is the number of components left in eq[i] after the filtering.  All that matters though is if
        //    eq[i] is now empty though.
        for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
        {
            FilterOneCompType(compareFunc,
                              compPair,
                              bitPos,
                              static_cast<MetaDataAddrComponentType>(compType),
                              axis);
        }

        if (IsEmpty(bitPos))
        {
            // This bit in the equation is now empty.  If there are still more significant valid bits to go,
            // then go ahead and shift everything down.
            const uint32  numBitsToGo = GetNumValidBits() - (bitPos + 1);
            if (numBitsToGo != 0)
            {
                // Shift everything above this position down
                memmove(&m_equation[bitPos][0],
                        &m_equation[bitPos + 1][0],
                        sizeof(uint32) * numBitsToGo * MetaDataAddrCompNumTypes);
            }

            // don't increment "bitPos" here since we just re-used that slot!

            // but do decrement the number of valid bits associated with this equation since there is
            // now one less
            m_maxBits--;
        }
        else
        {
            bitPos++;
        }
    } // end loop through all the bits in this equation
}

// =====================================================================================================================
// pEq is one bit of the meta-data equations; it will be indexed by this routine via the MetaDataAddrType enumerations.
//     i.e., pEq[] = x5 ^ y4
//
// This function will find and return the lowest coordinate that contributes to the supplied equation.  In this
// example:
//    pCompPair->type = MetaDataAddrCompY
//    pCompPair->pos  = 4;
//
// If pCompPair is valid, then this function will return true, otherwise, it will return false.
bool MetaDataAddrEquation::FindSmallComponent(
    uint32         bitPos,    // the bit position of the equation to look at
    CompPair*      pCompPair  // [out] the found small component
    ) const
{
    const uint32*  pEq = m_equation[bitPos];

    pCompPair->compType = MetaDataAddrCompNumTypes;
    pCompPair->compPos  = 0xFF;

    for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
    {
        // Iterate through each pipe bits from lsb to msb, and remove the smallest coordinate contributing
        // to that bit's equation
        uint32  lowCompPos = 0;
        if (BitMaskScanForward(&lowCompPos, pEq[compType]))
        {
            if (lowCompPos < pCompPair->compPos)
            {
                *pCompPair = SetCompPair(compType, lowCompPos);
            }
        }
    }

    return (pCompPair->compType != MetaDataAddrCompNumTypes);
}

// =====================================================================================================================
// This function returns the component associated with the specified bit.  i.e., if you have:
//    eq[0] = y3
//    eq[1] = x2
//
// it would return "y3" for bitPos==0.
//
// This assumes that there is only one component per bit.  i.e., these situations will assert:
//    eq[0] = y3 ^ y2
//    eq[0] = y3 ^ x2
CompPair MetaDataAddrEquation::Get(
    uint32  bitPos
    ) const
{
    bool      foundValidComp = false;
    CompPair  retPair        = {};

    for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
    {
        const uint32  data = Get(bitPos, compType);

        // Data of zero means that no components exist
        if (data != 0)
        {
            PAL_ASSERT (IsPowerOfTwo(data));
            PAL_ASSERT (foundValidComp == false);

            retPair = SetCompPair(compType, Log2(data));

            foundValidComp = true;
        }
    }

    // The requested bitPosition is empty?
    PAL_ASSERT (foundValidComp);

    return retPair;
}

// =====================================================================================================================
// This function returns the data associated with the specified equation bit and component
uint32 MetaDataAddrEquation::Get(
    uint32  bitPos,   // the bit position of the equation to look at
    uint32  compType  // one of MetaDataAddrCompType enumerations
    ) const
{
    ValidateInput(bitPos, compType);

    return m_equation[bitPos][compType];
}

// =====================================================================================================================
// Returns the number of bytes required to store this equation in GPU memory.
gpusize MetaDataAddrEquation::GetGpuSize() const
{
    return m_maxBits * MetaDataAddrCompNumTypes * sizeof (uint32);
}

// =====================================================================================================================
// Returns the number of samples that actually affect the final value of this equation.  Returns one if samples
// don't affect this equation's formula.
uint32 MetaDataAddrEquation::GetNumSamples() const
{
    uint32  highSampleBit = 0;

    for (uint32  bitPos = 0; bitPos < GetNumValidBits(); bitPos++)
    {
        const uint32  eqData = Get(bitPos, MetaDataAddrCompS);

        uint32  index = 0;
        if (BitMaskScanReverse(&index, eqData))
        {
            // Say the high reference in this equation is "s2".  This would be returned by this function as "1 << 2"
            // (which equals 4), but we really need to loop through the first seven samples in this case (i.e., up to
            // (1 << (2 + 1)) == 8 to ensure we catch all possibilities where s2 would be set.  i.e.,:
            //     0100
            //     0101
            //     0110
            //     0111
            //
            // Thus, we add "+ 1" here to the discovered index.
            highSampleBit = Max(highSampleBit, index + 1);
        }
    }

    return 1 << highSampleBit;
}

// =====================================================================================================================
// Returns true if the specified bit of this equation is empty.
bool MetaDataAddrEquation::IsEmpty(
    uint32  bitPos
    ) const
{
    return (GetNumComponents(bitPos) == 0);
}

// =====================================================================================================================
bool MetaDataAddrEquation::IsSet(
    uint32  bitPos,    // the bit position of the equation to look at
    uint32  compType,  // one of MetaDataAddrCompType enumerations
    uint32  mask       // the bit pattern to test for
    ) const
{
    return TestAnyFlagSet(Get(bitPos, compType), mask);
}

// =====================================================================================================================
void MetaDataAddrEquation::Mort2d(
    const Device* pGfxDevice,
    CompPair*     pPair0,
    CompPair*     pPair1,
    uint32        start,
    uint32        end)
{
    const Pal::Device&  palDevice = *(pGfxDevice->Parent());

    if (end == 0)
    {
        end = m_maxBits - 1;
    }

    if (IsGfx9(palDevice))
    {
        for (uint32 i = start; i <= end; i++)
        {
            CompPair*  pChosen = pPair0;

            if (((i - start) % 2) != 0)
            {
                pChosen = pPair1;
            }

            SetBit(i, pChosen->compType, pChosen->compPos);
            pChosen->compPos++;
        }
    }
    else if (IsGfx10Plus(palDevice))
    {
        const bool reverse = (end < start);

        for (uint32 i = start; (reverse) ? i >= end : i <= end; ((reverse) ? i-- : i++))
        {
            int select = ((reverse) ? (start - i) : (i - start)) % 2;

            CompPair*  pChosen = (select == 0) ? pPair0 : pPair1;
            SetBit(i, pChosen->compType, pChosen->compPos);
            pChosen->compPos++;
        }
    }
}

// =====================================================================================================================
void MetaDataAddrEquation::Mort3d(
    CompPair*  pC0,
    CompPair*  pC1,
    CompPair*  pC2,
    uint32     start,
    uint32     end)
{
    if (end == 0)
    {
        end = GetNumValidBits() - 1;
    }

    for(uint32 i = start; i <= end; i++)
    {
        const uint32  select = (i - start) % 3;
        auto*const    pC     = ((select == 0) ? pC0 : ((select==1) ? pC1 : pC2));

        SetBit(i, pC->compType, pC->compPos);
        pC->compPos++;
    }
}

// =====================================================================================================================
void MetaDataAddrEquation::PrintEquation(
    const Pal::Device*  pDevice
    ) const
{
#if PAL_ENABLE_PRINTS_ASSERTS
    const Gfx9PalSettings&  settings = GetGfx9Settings(*pDevice);
    if (TestAnyFlagSet(settings.printMetaEquationInfo, Gfx9PrintMetaEquationInfoEquations))
    {
        DbgPrintf(DbgPrintCatInfoMsg, DbgPrintStyleNoPrefix, "%s equation", m_equationName);

        for (uint32 bit = 0; bit < GetNumValidBits(); bit++)
        {
            constexpr const char CompNames[MetaDataAddrCompNumTypes] = { 'x', 'y', 'z', 's', 'm' };

            // Guessing?  I hope 256 is long enough.  :-)
            char  printMe[256] = {};

            for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
            {
                uint32  data      = m_equation[bit][compType];
                for (uint32 lowSetBit : BitIter32(data))
                {
                    static const uint32  CompNameSize = 16;
                    char  compName[CompNameSize] = {};

                    Snprintf(compName, CompNameSize, "%c%u ^ ", CompNames[compType], lowSetBit);
                    strcat (printMe, compName);
                }
            }

            // We wind up with one extra '^' character, so find it and remove it so the printout looks nicer
            char*  pChar = strrchr(printMe, '^');
            if (pChar)
            {
                *pChar = ' ';
            }

            DbgPrintf(DbgPrintCatInfoMsg, DbgPrintStyleNoPrefix, "\teq[%2d] = %s", bit, printMe);
        }
    }
#endif
}

// =====================================================================================================================
bool MetaDataAddrEquation::Remove(
    const CompPair&  compPair,
    uint32           bitPos)
{
    const uint32  mask        = 1 << compPair.compPos;
    bool          dataRemoved = false;

    if (TestAnyFlagSet(Get(bitPos, compPair.compType), mask))
    {
        ClearBits(bitPos, compPair.compType, ~mask);
        dataRemoved = true;
    }

    return dataRemoved;
}

// =====================================================================================================================
bool MetaDataAddrEquation::Remove(
    const CompPair&  compPair)
{
    bool  dataRemoved = false;

    for (uint32  bitPos = 0; bitPos < GetNumValidBits(); bitPos++)
    {
        dataRemoved |= Remove(compPair, bitPos);
    }

    return dataRemoved;
}

// =====================================================================================================================
void MetaDataAddrEquation::Reset()
{
    memset(m_equation, 0, sizeof(m_equation));
}

// =====================================================================================================================
void MetaDataAddrEquation::Reverse(
    uint32 start,
    int32  num)
{
    const uint32 n = (num == -1) ? GetNumValidBits() : num;

    for(uint32 bitPos = 0; bitPos < n/2; bitPos++)
    {
        for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
        {
            const uint32  temp        = Get(start + bitPos, compType);
            const uint32  otherBitPos = start + n - 1 - bitPos;

            ClearBits(start + bitPos, compType, 0);
            SetMask(start + bitPos, compType, Get(otherBitPos, compType));

            ClearBits(otherBitPos, compType, 0);
            SetMask(otherBitPos, compType, temp);
        }
    }
}

// =====================================================================================================================
void MetaDataAddrEquation::SetBit(
    uint32                     bitPos,
    MetaDataAddrComponentType  compType,
    uint32                     compPos)
{
    SetMask(bitPos, compType, 1 << compPos);
}

// =====================================================================================================================
void MetaDataAddrEquation::SetEquationSize(
    uint32  numBits,
    bool    clearBits)
{
    // This could conceivably trip for PRT images which can be ridiculously ginormous.  If so, we need to bump up
    // the MaxNumMetaDataAddrBits value.  Theoretically everything would simply "go along for the ride" with the
    // increased size.
    PAL_ASSERT(numBits <= MaxNumMetaDataAddrBits);

    // Only clear if caller requests it.
    if (clearBits)
    {
        // If there is anything leftover after the current equation finishes, then remove it
        for (uint32 bitPos = m_maxBits; bitPos < numBits; bitPos++)
        {
            ClearBitPos(bitPos);
        }
    }

    m_maxBits = numBits;
}

// =====================================================================================================================
void MetaDataAddrEquation::GenerateMetaEqParamConst(
    const Image&       image,
    uint32             maxCompFrag,
    uint32             firstUploadBit,
    MetaEquationParam* pMetaEqParam)
{
    const Pal::Image*      pParent     = image.Parent();
    const Pal::Device*     pDevice     = pParent->GetDevice();
    const Gfx9PalSettings& settings    = GetGfx9Settings(*pDevice);

    const bool optimizedFastClearDepth = ((pParent->IsDepthStencilTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearDepth));
    const bool optimizedFastClearDcc   = ((pParent->IsRenderTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearColorDcc));
    const bool optimizedFastClearCmask = ((pParent->IsRenderTarget()) &&
                                          TestAnyFlagSet(settings.optimizedFastClear,
                                                         Gfx9OptimizedFastClearColorCmask));

    // check if optimized fast clear is on.
    if (optimizedFastClearDepth || optimizedFastClearDcc || optimizedFastClearCmask)
    {
        // Meta Equation must have non zero bits
        PAL_ASSERT(m_maxBits);

        uint32 sampleHi = 0;
        uint32 sampleHiBitsLength = 0;

        uint32 metablkIdxLoBitsOffset = 0;
        uint32 metablkIdxLoBitsLength = 0;
        uint32 metablkIdxHiBitsOffset = 0;

        // Loop Over entire meta equation and find out SampleHi and MetaBlockHi and Lo bits
        for (uint32 bitPos = firstUploadBit; ((bitPos < MaxNumMetaDataAddrBits) && (bitPos < m_maxBits)); bitPos++)
        {
            // First check if any of bitPos has any sample bits.
            const uint32  sampleData = m_equation[bitPos][MetaDataAddrCompS];
            uint32  lowSetSampleBit = 0;

            // if sampleHi Bits haven't been found and a nonzero data has been found then
            // it must be lower bits of high sample bits. But its lsb must have a 1 which is
            // not at position 0 since s0 will come under compressed fragments
            if (sampleHi == 0)
            {
                if (BitMaskScanForward(&lowSetSampleBit, sampleData) && (lowSetSampleBit >= maxCompFrag))
                {
                    sampleHi = bitPos;
                    // If this is bitPos = m_maxBits - 1, meaning last valid bit in the equation
                    // then our below logic to find sampleHiBitsLength won't work so just update
                    // it here.
                    if (bitPos == (m_maxBits - 1))
                    {
                        sampleHiBitsLength = 1;
                    }
                }
            }
            else if ((sampleHi != 0) && (sampleData == 0) && (sampleHiBitsLength == 0))
            {
                sampleHiBitsLength = bitPos - sampleHi;
            }
            else if ((sampleHi != 0) && (sampleHiBitsLength == 0) && (bitPos == (m_maxBits - 1)))
            {
                sampleHiBitsLength = (bitPos - sampleHi) + 1;
            }

            // Now Find Metablock Lo and Hi bits
            const uint32  metaBlockData = m_equation[bitPos][MetaDataAddrCompM];
            uint32  lowSetMetaBlockBit = 0;

            if (metablkIdxLoBitsOffset == 0)
            {
                // Look for the m0 reference
                if ((metaBlockData & 0x1) != 0)
                {
                   metablkIdxLoBitsOffset = bitPos;
                }
            }
            else if ((metaBlockData == 0) && (metablkIdxLoBitsLength == 0))
            {
                // After metablock low bits has been found first non-occurance of any meta block bits
                // tell us about how many Low bits are present in the equation.
                metablkIdxLoBitsLength = bitPos - metablkIdxLoBitsOffset;
            }

            if ((metaBlockData != 0) && (metablkIdxLoBitsLength > 0) && (metablkIdxHiBitsOffset == 0))
            {
                // Find metablock hi bits offset
                metablkIdxHiBitsOffset = bitPos;
            }
        }

        if (sampleHiBitsLength == 0)
        {
            sampleHi = 0;
        }
        else
        {
            sampleHi--;
        }

        // if equation doesn't contain any metablock hi bits for example:-
        // x5 ^ y6,x6 ^ y5,x4 ^ y7, x7 ^ y4,x4 ^ y4 ^ z0,x5 ^ y3 ^ z1,x3 ^ y5 ^ z2, x6 ^ y2 ^ z3,m1,m0,y9,x8,y8,y7,x6,y6
        // then just assume that metablkIdxHiBitsOffset is at m_maxBits
        if (metablkIdxHiBitsOffset == 0)
        {
            metablkIdxHiBitsOffset = m_maxBits;
        }

        if (metablkIdxLoBitsLength == 0)
        {
            if (metablkIdxLoBitsOffset != 0)
            {
                metablkIdxHiBitsOffset = metablkIdxLoBitsOffset;
                metablkIdxLoBitsOffset = 0;
            }
            else
            {
                // Our trimming logic of meta equation see calcMetaEquation() may also sometime trim all metablock
                // bits even though actual meta equation will always contain atleast one bit of metablock. In this
                // case metablkIdxLoBitsOffset will come as 0, so handle it here. Assume it will sit just above the
                // last valid bit in the equation. If that is not the case something bad may happen.
                PAL_ASSERT(IsSet(m_maxBits, MetaDataAddrCompM, 1));

                metablkIdxHiBitsOffset = m_maxBits;
                metablkIdxLoBitsOffset = 0;
            }
        }
        else
        {
            metablkIdxLoBitsOffset--;
        }

        if (metablkIdxHiBitsOffset != 0)
        {
            metablkIdxHiBitsOffset--;
        }

        uint32 metaBlockFastClearSize = metablkIdxHiBitsOffset - metablkIdxLoBitsLength - sampleHiBitsLength;

        // Some sanity checks since we Convert uint from bytes to 16 bytes.
        PAL_ASSERT(metaBlockFastClearSize > 4);
        PAL_ASSERT(metablkIdxHiBitsOffset > 4);
        if ((sampleHiBitsLength > 0) && (sampleHi <= 4))
        {
            PAL_ASSERT_ALWAYS();
        }
        if ((metablkIdxLoBitsLength > 0) && (metablkIdxLoBitsOffset <= 4))
        {
            PAL_ASSERT_ALWAYS();
        }

        // Convert uint from bytes to 16 bytes
        pMetaEqParam->metaBlkSizeLog2 = metaBlockFastClearSize - 4;

        if (sampleHiBitsLength > 0)
        {
            pMetaEqParam->sampleHiBitsOffset = sampleHi - 4;
        }
        else
        {
            pMetaEqParam->sampleHiBitsOffset = 0;
        }

        pMetaEqParam->sampleHiBitsLength = sampleHiBitsLength;

        if (metablkIdxLoBitsLength > 0)
        {
            pMetaEqParam->metablkIdxLoBitsOffset = metablkIdxLoBitsOffset - 4;
        }
        else
        {
            pMetaEqParam->metablkIdxLoBitsOffset = 0;
        }

        pMetaEqParam->metablkIdxLoBitsLength = metablkIdxLoBitsLength;

        pMetaEqParam->metablkIdxHiBitsOffset = metablkIdxHiBitsOffset - 4;

        if ((pMetaEqParam->metaBlkSizeLog2 + pMetaEqParam->sampleHiBitsLength + pMetaEqParam->metablkIdxLoBitsLength) !=
            pMetaEqParam->metablkIdxHiBitsOffset)
        {
            PAL_ASSERT_ALWAYS();
        }
    }
}

// =====================================================================================================================
CompPair MetaDataAddrEquation::SetCompPair(
    MetaDataAddrComponentType  compType,
    uint32                     compPos)
{
    // Make sure our "compPos" is not out of range.  We use uint32's to store the equation, so any component
    // (i.e., x7) of the equation shouldn't reference more than the 32nd bit.
    if (compType == MetaDataAddrCompZ)
    {
        // Note that for Z, the compPos can be negative as part of the equation involves "metaBlkDepth - 1", and
        // the metablkDepth will be zero for 2D images.
        PAL_ASSERT ((static_cast<int32>(compPos) == -1) || (compPos < 32));
    }
    else
    {
        PAL_ASSERT (compPos < 32);
    }

    CompPair  compPair =
    {
        compType,
        static_cast<uint8>(compPos)
    };

    return compPair;
}

// =====================================================================================================================
CompPair MetaDataAddrEquation::SetCompPair(
    uint32  compType,
    uint32  compPos)
{
    return SetCompPair(static_cast<MetaDataAddrComponentType>(compType), compPos);
}

// =====================================================================================================================
void MetaDataAddrEquation::SetMask(
    uint32  bitPos,
    uint32  compType,
    uint32  mask)
{
    ValidateInput(bitPos, compType);

    if (IsEmpty(bitPos) && IsPowerOfTwo(mask))
    {
        m_firstPair[bitPos] = MetaDataAddrEquation::SetCompPair(compType, Util::Log2(mask));
    }

    // Set the requested bit(s) in the equation
    m_equation[bitPos][compType] |= mask;
}

// =====================================================================================================================
void MetaDataAddrEquation::Shift(
    int32  amount, // the number of equation bits to shift, negative values are a left shift
    int32  start)  // right-shifts only, the first bit to move
{
    if (amount != 0)
    {
        amount = -amount;

        const int32  inc = (amount < 0) ? -1        : 1;
        const int32  end = (amount < 0) ? start - 1 : GetNumValidBits();

        for (int32 bitPos = (amount < 0) ? GetNumValidBits() - 1 : start;
             (inc > 0) ? bitPos < end : bitPos > end;
             bitPos += inc)
        {
            if ((bitPos + amount < start) || (bitPos + amount >= static_cast<int32>(GetNumValidBits())))
            {
                memset (m_equation[bitPos], 0, sizeof(uint32) * MetaDataAddrCompNumTypes);
            }
            else
            {
                memcpy (m_equation[bitPos], m_equation[bitPos + amount], sizeof(uint32) * MetaDataAddrCompNumTypes);
            }
        }
    }
}

// =====================================================================================================================
// Uploads this objects equation to GPU-accessible memory
void MetaDataAddrEquation::Upload(
    const Pal::Device*  pDevice,
    CmdBuffer*          pCmdBuffer,
    const GpuMemory&    dstMem,     // Mem object that the equation is written into
    gpusize             offset,     // Offset from dstMem to which the equation gets written
    uint32              firstbit    // [in] the LSB of the equation that we actually care about
    ) const
{
    // Make sure all the bits that we're NOT uploading will always be zero.
    for (uint32  bitPos = 0; bitPos < firstbit; bitPos++)
    {
        PAL_ASSERT(IsEmpty(bitPos));
    }

    // Always write all possible components for each bit of the equation (even if they're empty)
    pCmdBuffer->CmdUpdateMemory(dstMem,
                                offset,
                                MetaDataAddrCompNumTypes * (GetNumValidBits() - firstbit) * sizeof(uint32),
                                &m_equation[firstbit][0]);

    if (pCmdBuffer->GetEngineType() != EngineTypeDma)
    {
        const auto*  pGfxDevice    = static_cast<const Device*>(pDevice->GetGfxDevice());
        auto*        pGfxCmdBuffer = static_cast<Pm4CmdBuffer*>(pCmdBuffer);
        auto*        pCmdStream    = pGfxCmdBuffer->GetCmdStreamByEngine(CmdBufferEngineSupport::CpDma);
        auto*        pGfxCmdStream = static_cast<CmdStream*>(pCmdStream);

        PAL_ASSERT(pCmdStream != nullptr);

        // The following code assumes that the above CmdUpdateMemory() call utilized the CPDMA engine.
        //
        // We have to guarantee that the CPDMA operation has completed as the texture pipe will (conceivably) be
        // using this equation "real soon now". See the RPM "InitMaskRam" implementation for details.
        SyncReqs  syncReqs = {};
        syncReqs.syncCpDma = 1;

        // Dummy BarrierOperations used in Device::IssueSyncs()
        Developer::BarrierOperations barrierOps = {};
        pGfxDevice->BarrierMgr()->IssueSyncs(pGfxCmdBuffer, pGfxCmdStream, syncReqs, HwPipePoint::HwPipePreCs,
                                             0, 0, &barrierOps);
    }
    else
    {
        // For SDMA-based uploads, the PAL client is responsible for issuing barrier calls that ensure the completion
        // of the SDMA engine prior to the texture pipe getting involved so there's nothing we need to do.
    }
}

// =====================================================================================================================
void MetaDataAddrEquation::ValidateInput(
    uint32  bitPos,
    uint32  compType
    ) const
{
    PAL_ASSERT (bitPos   < MaxNumMetaDataAddrBits);
    PAL_ASSERT (compType < MetaDataAddrCompNumTypes);
}

// =====================================================================================================================
// Adds everything from pEq into "this"
void MetaDataAddrEquation::XorIn(
    const MetaDataAddrEquation*  pEq,
    uint32                       start)
{
    const uint32 numBits = (GetNumValidBits() - start < pEq->GetNumValidBits())
                           ? GetNumValidBits() - start
                           : pEq->GetNumValidBits();

    for (uint32 bitPos = 0; bitPos < numBits; bitPos++)
    {
        for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
        {
            this->SetMask(bitPos, compType, pEq->Get(bitPos, compType));
        }
    }
}

// =====================================================================================================================
// Returns true if the meta equation bit specified by the "this" object's "thisBit" is equivalent to the
// metaEq's "metaBit"
bool MetaDataAddrEquation::IsEqual(
    const MetaDataAddrEquation&  metaEq,
    uint32                       thisBit,
    uint32                       metaBit
    ) const
{
    bool isEqual = true;

    for (uint32  compType = 0; isEqual && (compType < MetaDataAddrCompNumTypes); compType++)
    {
        isEqual = (metaEq.Get(metaBit, compType) == Get(thisBit, compType));
    }

    return isEqual;
}

// =====================================================================================================================
// Returns the number of components referenced by the specified bit.
uint32 MetaDataAddrEquation::GetNumComponents(
    uint32  bitPos
    ) const
{
    uint32  numComponents = 0;

    for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
    {
        const uint32  data = Get(bitPos, compType);

        numComponents += CountSetBits(data);
    }

    return numComponents;
}

// =====================================================================================================================
void MetaDataAddrEquation::AdjustPipe(
    int32  numPipesLog2,
    int32  offset,
    bool   undo)
{
    const int32 i = offset;
    const int32 j = offset + numPipesLog2 - 1;
    const int32 r = (undo) ? 1 : -1;

    Rotate(r, i, j);
}

// =====================================================================================================================
void MetaDataAddrEquation::Rotate(
    int32 amount,
    int32 start,
    int32 end)
{
    if (end == -1)
    {
        // Go with the first empty bit in the equation.  "GetNumValidBits" is the total number of possible bits
        // in the equation.  Go backwards since the first couple bits will be empty.
        bool  lastValidFound = false;
        for (end = GetNumValidBits() - 1; ((lastValidFound == false) && (end >= 0)); end--)
        {
            lastValidFound = (IsEmpty(end) == false);
        }

        // "end" is now the last non-empty bit in the equation; we want the first empty bit.
        end++;
    }

    const int32 size = 1 + end - start;
    MetaDataAddrEquation  rotCopy(size, "rotCopy");

    Copy(&rotCopy, start, size);
    for (int32 i = 0; i < size; i++)
    {
        int32 src = (i - amount);
        if (src < 0)
        {
            src = -src % size;
            src = size - src;
        }
        else
        {
            src = src % size;
        }

        const uint32  dstBitPos = start + i;
        ClearBitPos(dstBitPos);
        for (uint32 compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
        {
            SetMask(dstBitPos, compType, rotCopy.Get(src, compType));
        }
    }
}

// =====================================================================================================================
// Swap the equation data located in "pos1" and "pos2"
void MetaDataAddrEquation::Swap(
    uint32  pos1,
    uint32  pos2)
{
    for (uint32  compType = 0; compType < MetaDataAddrCompNumTypes; compType++)
    {
        const uint32  newPos2Data = m_equation[pos1][compType];
        const uint32  newPos1Data = m_equation[pos2][compType];

        m_equation[pos2][compType] = newPos2Data;
        m_equation[pos1][compType] = newPos1Data;
    }
}

} // Gfx9
} // Pal
