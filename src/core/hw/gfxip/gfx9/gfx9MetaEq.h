/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "pal.h"

namespace Pal
{

namespace Gfx9
{
class Device;
class Image;

// These are the component types that can go into generating any one bit of the final equation.  The ordering of
// this is important.  i.e., this list is ordered from "most important" to "least important" component types.
enum MetaDataAddrComponentType : uint32
{
    MetaDataAddrCompX,
    MetaDataAddrCompY,
    MetaDataAddrCompZ,  // slice
    MetaDataAddrCompS,  // sample
    MetaDataAddrCompM,  // meta-block
    MetaDataAddrCompNumTypes,
};

// Types of comparisons that the "Compare" function can do.
enum MetaDataAddrCompareTypes : uint32
{
    MetaDataAddrCompareLt, // Less Than
    MetaDataAddrCompareGt, // Greater than
    MetaDataAddrCompareEq, // Equals
};

// =====================================================================================================================
// parameters extracted out of meta equation of a given meta data. All these parameters in 16byte address granularity
// metadata addressing pattern can be thought of as divided into two schemes:-
// A.Metablock[Hi], Sample[Hi], CombinedOffset[Hi], Metablock[Lo], CombinedOffset[Lo] :- When addressing is both Pipe
// and RB Aligned.
// B.Metablock[all], CombinedOffset[Hi], Sample[Hi], CombinedOffset[Lo] :- When addressing is only RB Aligned.
// In our implementation it is mostly A since we request meta data to be both Pipe and RB aligned.
struct MetaEquationParam
{
    uint32 metaBlkSizeLog2;        // Sum of CombinedOffset[Hi] and CombinedOffset[L0]
    uint32 sampleHiBitsOffset;     // Offset of Sample bits above MaxCompressFrag supported by the Asic.
    uint32 sampleHiBitsLength;     // Number of Sample bits above MaxCompressFrag supported by the Asic.
    uint32 metablkIdxLoBitsOffset; // Metablock[Lo]:- LSB in meta equation
    uint32 metablkIdxLoBitsLength; // MetaBlock[Lo]:- Number of metablock bits split by below rb/pipe equations.
    uint32 metablkIdxHiBitsOffset; // Metablock[Hi]:- LSB in meta equation above rb/pipe equations.
};

// =====================================================================================================================
// One comp-pair is a single element -- i.e., something like "x5".
struct CompPair
{
    MetaDataAddrComponentType  compType;
    uint8                      compPos;
};

// In some situations the component position can be a negative number, but we're storing it as an unsigned integer
// 0xFF = -1 when interpreted as a signed number which is what the CompareCompPair function does.
static const uint8  MinMetaEqCompPos = 0xFF;

// =====================================================================================================================
// One instance of a "MetaDataAddrEquation" object is one equation -- i.e., all the bits.
// One equation is something like:
//    eq[1] = x5 ^ y5
//    eq[0] = x4 ^ y4 ^ y5
//
// This means (obviously) that the equation produces a two-bit number where:
//    eq[1] = (x & (1 << 5)) XOR (y & (1 << 5))
//    eq[0] = (x & (1 << 4)) XOR (y & (1 << 4)) XOR (y & (1 << 5))
//
// The routines in this class are for manipulating the equation.
//    "bitPos"   is used to indicate an index into the equation.
//    "compType" is the component type, one of the MetaDataAddrComponentType enumerations
//    "compPos"  is the bit position of a given component.
//    "mask"     is used to describe which bit(s) of a component are "interesting".
//
// i.e., for "x5":
//         compType = MetaDataAddrCompX
//         compPos  = 5
//         mask     = 1 << 5
//
//
class MetaDataAddrEquation
{
public:
    MetaDataAddrEquation(uint32  maxEquationBits, const char*  pName = nullptr);
    virtual ~MetaDataAddrEquation() {}

    // This is the maximum number of bits that any given equation can produce
    static const uint32  MaxNumMetaDataAddrBits = 32;

    void ClearBits(
        uint32   bitPos,
        uint32   compType,
        uint32   keepMask);
    void Copy(
        MetaDataAddrEquation*  pDst,
        uint32                 startBitPos   = 0,
        int32                  numBitsToCopy = -1) const;
    uint32 CpuSolve(
        uint32  x,
        uint32  y,
        uint32  z,
        uint32  sample,
        uint32  metaBlock) const;
    bool Exists(
        uint32  compType,
        uint32  data) const;
    void Filter(
        const CompPair&            compPair,
        MetaDataAddrCompareTypes   compareFunc,
        uint32                     startBit = 0,
        MetaDataAddrComponentType  axis = MetaDataAddrCompNumTypes);
    bool FindSmallComponent(
        uint32     bitPos,
        CompPair*  pCompPair) const;
    CompPair Get(
        uint32  bitPos) const;
    uint32 Get(
        uint32  bitPos,
        uint32  compType) const;
    uint32 GetNumSamples() const;
    gpusize GetGpuSize() const;
    bool IsEqual(
        const   MetaDataAddrEquation&  metaEq,
        uint32  thisBit,
        uint32  metaBit) const;
    bool IsEmpty(uint32  bitPos) const;
    bool IsSet(
        uint32  bitPos,
        uint32  compType,
        uint32  mask) const;
    void Mort2d(
        CompPair*  pPair0,
        CompPair*  pPair1,
        uint32     start = 0,
        uint32     end   = 0);
    void Mort3d(
        CompPair*  pC0,
        CompPair*  pC1,
        CompPair*  pC2,
        uint32     start = 0,
        uint32     end = 0);
    void PrintEquation(const Pal::Device*  pDevice) const;
    bool Remove(const CompPair&  compPair);
    void GenerateMetaEqParamConst(
        const Image&       image,
        uint32             maxCompFrag,
        uint32             firstUploadBit,
        MetaEquationParam* pMetaEqParam);
    void Reset();
    void Reverse(
        uint32 start = 0,
        int    num   = -1);
    void SetBit(
        uint32                     bitPos,
        MetaDataAddrComponentType  compType,
        uint32                     compPos);
    static CompPair SetCompPair(
        MetaDataAddrComponentType  compType,
        uint32                     compPos);
    static CompPair SetCompPair(
        uint32  compType,
        uint32  compPos);
    void SetEquationSize(uint32  numBits, bool clearBits = true);
    void SetMask(
        uint32  bitPos,
        uint32  compType,
        uint32  mask);
    void Shift(
        int32  amount,
        int32  start = 0);
    void Upload(
        const Pal::Device*  pDevice,
        CmdBuffer*          pCmdBuffer,
        const GpuMemory&    dstMem,
        gpusize             offset,
        uint32              firstbit) const;
    void XorIn(
        const MetaDataAddrEquation*  pEq,
        uint32                       start = 0);

    uint32 GetNumValidBits() const { return m_maxBits; }
    uint32 GetNumComponents(uint32  bitPos) const;

    static bool CompareCompPair(
        const CompPair&           pair0,
        const CompPair&           pair1,
        MetaDataAddrCompareTypes  compareType);

private:
    void FilterOneCompType(
        MetaDataAddrCompareTypes   compareFunc,
        const CompPair&            compPair,
        uint32                     bitPos,
        MetaDataAddrComponentType  compType,
        MetaDataAddrComponentType  axis);

    void ValidateInput(uint32  bitPos, uint32  compType) const;

#if PAL_ENABLE_PRINTS_ASSERTS
    static const uint32  MaxEquationNameLength = 32;
    char    m_equationName[MaxEquationNameLength]; // The name given to this equation, used only for printing
#endif

    uint32  m_maxBits;                             // The maximum number of bits this equation could have

    // One of the meta-data address equations.
    //    equation[0][MetaDataAddrCompX] = 0x5 would mean that the "X" component of bit 0 of this equation is
    //                                     composed of x4 ^ x1;
    //
    // i.e., each uint32 is a bitmask indicating where each set bit position indicates which bits of the component
    //       are important for the final equation bit.  (Got that?).  The "final equation bit" is the first index
    //       into the array, the "component" is the second index into the array.
    uint32  m_equation[MaxNumMetaDataAddrBits][MetaDataAddrCompNumTypes];
};

} // Gfx9
} // Pal
