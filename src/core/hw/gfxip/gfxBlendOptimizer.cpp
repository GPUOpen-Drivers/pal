/*
 *******************************************************************************
 *
 * Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "core/hw/gfxip/gfxBlendOptimizer.h"
#include "palInlineFuncs.h"

namespace Pal
{
namespace GfxBlendOptimizer
{

// States indicating value requirements for different color/alpha components to make the optimizations work.
enum class ValueReq : uint32
{
    DontCare = 0, // Optimization doesn't depend on the value
    Need0,        // Need value of zero for optimization to work
    Need1,        // Need value of one for optimization to work
};

// Indices into different parts of blend equations for referencing value requirements
enum ValueReqSelectIndx : uint32
{
    ValueReqSelectSrcColor = 0, // Source color
    ValueReqSelectDestColor,    // Destination color
    ValueReqSelectSrcAlpha,     // Source alpha
    ValueReqSelectDestAlpha,    // Destination alpha
    ValueReqSelectIndexCount,   // Number of blend equation parts
};

// Intermediate optimization state used in optimization equation evaluation
struct OptState
{
    ValueReq reqColorVal[ValueReqSelectIndexCount]; // Req'ed values for source color
    ValueReq reqAlphaVal[ValueReqSelectIndexCount]; // Req'ed values for source alpha
    bool     canOptimize;                           // Global flag if optimization is possible
};

// =====================================================================================================================
// Prepare internal state for blend operation analysis.
static void InitOptState(
    OptState* pOptState)  // [out] Internal optimization state.
{
    memset(pOptState, 0, sizeof(*pOptState));

    for (uint32 i = 0; i < ValueReqSelectIndexCount; i++)
    {
        pOptState->reqColorVal[i] = ValueReq::DontCare;
        pOptState->reqAlphaVal[i] = ValueReq::DontCare;
    }

    // Initially assume that optimization can be applied
    pOptState->canOptimize = true;
}

// =====================================================================================================================
// Reduces multiple value requirements for optimization to a single requirement and returns it.  If conflicting
// requirements are detected, return false.  The combined value requirement is returned in *TotalReq.
static bool ReduceValueReqs(
    const ValueReq reqVal[ValueReqSelectIndexCount],
    ValueReq*      pTotalReq)
{
    bool canOptimize = true;

    (*pTotalReq) = reqVal[0];
    for (uint32 i = 1; i < ValueReqSelectIndexCount; i++)
    {
        if ((*pTotalReq) == ValueReq::DontCare)
        {
            (*pTotalReq) = reqVal[i];
        }
        else if (((*pTotalReq) != reqVal[i]) && (reqVal[i] != ValueReq::DontCare))
        {
            // Conflicting requirements detected, can't optimize.
            canOptimize = false;
            break;
        }
    }

    return canOptimize;
}

// =====================================================================================================================
// Map blend source value requirements to the HW blend optimization mode.
static BlendOpt SelectBlendOpt(
    const OptState& optState)  // Internal state with value requirements.
{
    BlendOpt opt = BlendOpt::ForceOptAuto;

    bool canOptimize  = optState.canOptimize;
    ValueReq colorReq = ValueReq::DontCare;
    ValueReq alphaReq = ValueReq::DontCare;

    if (canOptimize)
    {
        // Combine color value requirements
        canOptimize = ReduceValueReqs(optState.reqColorVal, &colorReq);
    }

    if (canOptimize)
    {
        // Combine alpha value requirements
        canOptimize = ReduceValueReqs(optState.reqAlphaVal, &alphaReq);
    }

    if (canOptimize)
    {
        // If no value conflits are found, try to map requirements to HW modes
        if ((colorReq == ValueReq::DontCare) && (alphaReq == ValueReq::Need0))
        {
            opt = BlendOpt::ForceOptEnableIfSrcA0;
        }
        else if ((colorReq == ValueReq::Need0) && (alphaReq == ValueReq::DontCare))
        {
            opt = BlendOpt::ForceOptEnableIfSrcRgb0;
        }
        else if ((colorReq == ValueReq::Need0) && (alphaReq == ValueReq::Need0))
        {
            opt = BlendOpt::ForceOptEnableIfSrcArgb0;
        }
        else if ((colorReq == ValueReq::DontCare) && (alphaReq == ValueReq::Need1))
        {
            opt = BlendOpt::ForceOptEnableIfSrcA1;
        }
        else if ((colorReq == ValueReq::Need1) && (alphaReq == ValueReq::DontCare))
        {
            opt = BlendOpt::ForceOptEnableIfSrcRgb1;
        }
        else if ((colorReq == ValueReq::Need1) && (alphaReq == ValueReq::Need1))
        {
            opt = BlendOpt::ForceOptEnableIfSrcArgb1;
        }
    }

    return opt;
}

// =====================================================================================================================
// Evaluate requirements for the blending mode and figure out what source values could be used for optimization.
// Process a case when "dst*dstBlend +/- src*srcBlend" can be evaluated as "dst*1 +/- 0"
//
// TODO: Optimize cases when source could be either 0 or 1.
BlendOpt OptimizePixDiscard1(
    const Input& state)  // Blend state to evaluate.
{
    BlendOpt opt = BlendOpt::ForceOptAuto;

    // Don't clear optimization state on creation since InitOptState will do it.
    OptState optState;
    InitOptState(&optState);

    if (state.colorWrite)
    {
        // The arrays reqColorVal and reqAlphaVal below impose restrictions on specific values of srcColor and srcAlpha
        // (respectively) before the blend optimization can trigger.  These arrays are indexed by different locations
        // of the blend equation where the srcColor/srcAlpha value can appear, either as part of a specific blend
        // factor that references it, or when it is part of the blend equation directly.  The locations are:
        //
        // reqColor/AlphaVal[ValueReqSelectSrcColor]  = Req. srcColor/srcAlpha in the srcColor*srcBlend factor
        // reqColor/AlphaVal[ValueReqSelectDestColor] = Req. srcColor/srcAlpha in the destColor*destBlend factor
        // reqColor/AlphaVal[ValueReqSelectSrcAlpha]  = Req. srcColor/srcAlpha in the srcAlpha*alphaSrcBlend factor
        // reqColor/AlphaVal[ValueReqSelectDestAlpha] = Req. srcColor/srcAlpha in the destAlpha*alphaDestBlend factor
        //
        // Note also that "ValueReqSelectDestColor" does not refer to required values of the destination color.  Only
        // srcColor/srcAlpha is ever checked for known values in order to trigger the optimization.

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //      srcColor * srcBlend = 0

        switch (state.srcBlend)
        {
        case BlendOp::BlendZero:
            // srcColor * (0) = 0
            break;
        case BlendOp::BlendOneMinusSrcColor:
            // {1} * (1 - {1}) = 0
            optState.reqColorVal[ValueReqSelectSrcColor] = ValueReq::Need1;
            break;
        case BlendOp::BlendSrcAlpha:
            // srcColor * ({0}) = 0
        case BlendOp::BlendSrcAlphaSaturate:
            // srcColor * (min({0}, 1 - destAlpha)) = 0
            optState.reqAlphaVal[ValueReqSelectSrcColor] = ValueReq::Need0;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // srcColor * (1 - {1}) = 0
            optState.reqAlphaVal[ValueReqSelectSrcColor] = ValueReq::Need1;
            break;
        default:
            // {0} * anything = 0
            optState.reqColorVal[ValueReqSelectSrcColor] = ValueReq::Need0;
            break;
        }

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      destColor * destBlend = destColor

        switch (state.destBlend)
        {
        case BlendOp::BlendOne:
            // destColor * 1 = destColor
            break;
        case BlendOp::BlendSrcColor:
            // destColor * {1} = destColor
            optState.reqColorVal[ValueReqSelectDestColor] = ValueReq::Need1;
            break;
        case BlendOp::BlendOneMinusSrcColor:
            // destColor * (1 - {0}) = destColor
            optState.reqColorVal[ValueReqSelectDestColor] = ValueReq::Need0;
            break;
        case BlendOp::BlendSrcAlpha:
            // destColor * ({1}) = destColor
            optState.reqAlphaVal[ValueReqSelectDestColor] = ValueReq::Need1;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // destColor * (1 - {0}) = destColor
            optState.reqAlphaVal[ValueReqSelectDestColor] = ValueReq::Need0;
            break;
        default:
            // can't make any guarantees
            optState.canOptimize = false;
            break;
        }
    }

    if (state.alphaWrite)
    {
        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      srcAlpha * alphaSrcBlend = 0

        switch (state.alphaSrcBlend)
        {
        case BlendOp::BlendZero:
            // srcAlpha * 0 = 0
            break;
        case BlendOp::BlendSrcAlpha:
            // srcAlpha * {0} = 0
        case BlendOp::BlendSrcAlphaSaturate:
            // {0} * (1) = 0 // Note: src_alpha_saturate = 1 in alpha blend functions
            optState.reqAlphaVal[ValueReqSelectSrcAlpha] = ValueReq::Need0;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // srcAlpha * (1 - {1}) = 0
            optState.reqAlphaVal[ValueReqSelectSrcAlpha] = ValueReq::Need1;
            break;
        default:
            // {0} * anything = 0
            optState.reqAlphaVal[ValueReqSelectSrcAlpha] = ValueReq::Need0;
            break;
        }

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      destAlpha * alphaDestBlend = destAlpha

        switch (state.alphaDestBlend)
        {
        case BlendOp::BlendOne:
            // destAlpha * 1 = destAlpha
        case BlendOp::BlendSrcAlphaSaturate:
            // destAlpha * (1) = destAlpha // Note: src_alpha_saturate = 1 in alpha blend functions
            break;
        case BlendOp::BlendSrcAlpha:
            // destAlpha * ({1}) = destAlpha
            optState.reqAlphaVal[ValueReqSelectDestAlpha] = ValueReq::Need1;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // destAlpha * (1 - {0}) = destAlpha
            optState.reqAlphaVal[ValueReqSelectDestAlpha] = ValueReq::Need0;
            break;
        default:
            optState.canOptimize = false;
            break;
        }
    }

    // If optimization is possible, map value requirements to HW mode
    if (optState.canOptimize)
    {
        opt = SelectBlendOpt(optState);
    }

    return opt;
}

// =====================================================================================================================
// Evaluate requirements for the blending mode and figure out what source values could be used for optimization.
// Process a case when "src*srcBlend + dst*dstBlend" can be evaluated as "1*dst + dst*0"
BlendOpt OptimizePixDiscard2(
    const Input& state)  // Blend state to evaluate.
{
    BlendOpt opt = BlendOpt::ForceOptAuto;

    // Don't clear optimization state on creation since InitOptState will do it.
    OptState optState;
    InitOptState(&optState);

    if (state.colorWrite)
    {
        // The arrays reqColorVal and reqAlphaVal below impose restrictions on specific values of srcColor and srcAlpha
        // (respectively) before the blend optimization can trigger.  These arrays are indexed by different locations
        // of the blend equation where the srcColor/srcAlpha value can appear, either as part of a specific blend
        // factor that references it, or when it is part of the blend equation directly.  The locations are:
        //
        // reqColor/AlphaVal[ValueReqSelectSrcColor]  = Req. srcColor/srcAlpha in the srcColor*srcBlend factor
        // reqColor/AlphaVal[ValueReqSelectDestColor] = Req. srcColor/srcAlpha in the destColor*destBlend factor
        // reqColor/AlphaVal[ValueReqSelectSrcAlpha]  = Req. srcColor/srcAlpha in the srcAlpha*alphaSrcBlend factor
        // reqColor/AlphaVal[ValueReqSelectDestAlpha] = Req. srcColor/srcAlpha in the destAlpha*alphaDestBlend factor
        //
        // Note also that "ValueReqSelectDestColor" does not refer to required values of the destination color.  Only
        // srcColor/srcAlpha is ever checked for known values in order to trigger the optimization.

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      srcColor * srcBlend = destColor

        switch (state.srcBlend)
        {
        case BlendOp::BlendDstColor:
            // {1} * destColor = destColor
            optState.reqColorVal[ValueReqSelectSrcColor] = ValueReq::Need1;
            break;
        default:
            optState.canOptimize = false;
            break;
        }

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      destColor * destBlend = 0

        switch (state.destBlend)
        {
        case BlendOp::BlendZero:
            // destColor * 0 = 0
            break;
        case BlendOp::BlendSrcColor:
            // destColor * {0} = 0
            optState.reqColorVal[ValueReqSelectDestColor] = ValueReq::Need0;
            break;
        case BlendOp::BlendOneMinusSrcColor:
            // destColor * (1 - {1}) = 0
            optState.reqColorVal[ValueReqSelectDestColor] = ValueReq::Need1;
            break;
        case BlendOp::BlendSrcAlpha:
            // destColor * ({0}) = 0
        case BlendOp::BlendSrcAlphaSaturate:
            // destColor * (min({0}, 1 - dstAlpha)) = 0
            optState.reqAlphaVal[ValueReqSelectDestColor] = ValueReq::Need0;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // destColor * (1 - {1}) = 0
            optState.reqAlphaVal[ValueReqSelectDestColor] = ValueReq::Need1;
            break;
        default:
            optState.canOptimize = false;
            break;
        }
    }

    if (state.alphaWrite)
    {
        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      srcAlpha * alphaSrcBlend = destAlpha

        switch (state.alphaSrcBlend)
        {
        case BlendOp::BlendDstAlpha:
            // {1} * destAlpha = destAlpha
            optState.reqAlphaVal[ValueReqSelectSrcAlpha] = ValueReq::Need1;
            break;
        default:
            optState.canOptimize = false;
            break;
        }

        // The goal here is to figure out under what srcColor/srcAlpha values we can guarantee:
        //
        //      destAlpha * alphaDestBlend = 0

        switch (state.alphaDestBlend)
        {
        case BlendOp::BlendZero:
            // destAlpha * 0 = 0
            break;
        case BlendOp::BlendSrcAlpha:
            // destAlpha * {0} = 0
            optState.reqAlphaVal[ValueReqSelectDestAlpha] = ValueReq::Need0;
            break;
        case BlendOp::BlendOneMinusSrcAlpha:
            // destAlpha * (1 - {1}) = 0
            optState.reqAlphaVal[ValueReqSelectDestAlpha] = ValueReq::Need1;
            break;
        default:
            // Note: src_alpha_saturate = 1 in alpha blend functions
            optState.canOptimize = false;
            break;
        }
    }

    // If optimization is possible, map value requirements to HW mode
    if (optState.canOptimize)
    {
        opt = SelectBlendOpt(optState);
    }

    return opt;
}

} // GfxBlendOptimizer
} // Pal
