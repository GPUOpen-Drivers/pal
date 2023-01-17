/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"
#include "core/hw/gfxip/gfx9/gfx9DepthStencilState.h"

namespace Pal
{
namespace Gfx9
{

// =====================================================================================================================
DepthStencilState::DepthStencilState(
    const DepthStencilStateCreateInfo& createInfo)
    :
    Pal::DepthStencilState()
{
    memset(&m_flags, 0, sizeof(m_flags));

    m_dbDepthControl.u32All   = 0;
    m_dbStencilControl.u32All = 0;

    Init(createInfo);
}

// =====================================================================================================================
// Helper method to determine if a depth/stencil test operation allows out of order rendering
static bool CanRunOutOfOrder(
    CompareFunc func)
{
    return ((func == CompareFunc::Less)      ||
            (func == CompareFunc::Greater)   ||
            (func == CompareFunc::Equal)     ||
            (func == CompareFunc::LessEqual) ||
            (func == CompareFunc::GreaterEqual));
}

// =====================================================================================================================
// Performs Gfx9 hardware-specific initialization for a depth/stencil state object, including:
// Set up the image of PM4 commands used to write the pipeline to HW.
void DepthStencilState::Init(
    const DepthStencilStateCreateInfo& dsState)
{
    m_flags.isDepthEnabled   = dsState.depthEnable;
    m_flags.isStencilEnabled = dsState.stencilEnable;

    m_flags.isDepthWriteEnabled =
        dsState.depthEnable      &&
        dsState.depthWriteEnable &&
        (dsState.depthFunc != CompareFunc::Never);

    m_flags.isStencilWriteEnabled =
        dsState.stencilEnable           &&
        ((dsState.front.stencilFailOp != Pal::StencilOp::Keep)      ||
         (dsState.front.stencilPassOp != Pal::StencilOp::Keep)      ||
         (dsState.front.stencilDepthFailOp != Pal::StencilOp::Keep) ||
         (dsState.back.stencilFailOp != Pal::StencilOp::Keep)       ||
         (dsState.back.stencilPassOp != Pal::StencilOp::Keep)       ||
         (dsState.back.stencilDepthFailOp != Pal::StencilOp::Keep));

    m_flags.canDepthRunOutOfOrder =
        (dsState.depthEnable == false)         ||
        (m_flags.isDepthWriteEnabled == false) ||
        CanRunOutOfOrder(dsState.depthFunc);

    m_flags.canStencilRunOutOfOrder =
        (dsState.stencilEnable == false)             ||
        (m_flags.isStencilWriteEnabled == false)     ||
        (CanRunOutOfOrder(dsState.front.stencilFunc) &&
         CanRunOutOfOrder(dsState.back.stencilFunc));

    m_flags.depthForcesOrdering =
        dsState.depthEnable                        &&
        (dsState.depthFunc != CompareFunc::Always) &&
        (dsState.depthFunc != CompareFunc::NotEqual);

    // Setup DB_DEPTH_CONTROL.
    m_dbDepthControl.bits.Z_ENABLE       = (dsState.depthEnable ? 1 : 0);
    m_dbDepthControl.bits.Z_WRITE_ENABLE = (dsState.depthWriteEnable ? 1 : 0);
    m_dbDepthControl.bits.ZFUNC          = HwDepthCompare(dsState.depthFunc);

    m_dbDepthControl.bits.STENCIL_ENABLE = (dsState.stencilEnable ? 1 : 0);
    m_dbDepthControl.bits.STENCILFUNC    = HwStencilCompare(dsState.front.stencilFunc);
    m_dbDepthControl.bits.STENCILFUNC_BF = HwStencilCompare(dsState.back.stencilFunc);

    m_dbDepthControl.bits.DEPTH_BOUNDS_ENABLE = (dsState.depthBoundsEnable ? 1 : 0);
    // NOTE: Always on
    m_dbDepthControl.bits.BACKFACE_ENABLE = 1;

    // Force off as this is not linked to any API features. Their need/use is unclear.
    m_dbDepthControl.bits.ENABLE_COLOR_WRITES_ON_DEPTH_FAIL  = 0;
    m_dbDepthControl.bits.DISABLE_COLOR_WRITES_ON_DEPTH_PASS = 0;

    // Setup DB_STENCIL_CONTROL.

    // front stencil
    m_dbStencilControl.bits.STENCILFAIL  = HwStencilOp(dsState.front.stencilFailOp);
    m_dbStencilControl.bits.STENCILZFAIL = HwStencilOp(dsState.front.stencilDepthFailOp);
    m_dbStencilControl.bits.STENCILZPASS = HwStencilOp(dsState.front.stencilPassOp);

    // back stencil
    m_dbStencilControl.bits.STENCILFAIL_BF  = HwStencilOp(dsState.back.stencilFailOp);
    m_dbStencilControl.bits.STENCILZFAIL_BF = HwStencilOp(dsState.back.stencilDepthFailOp);
    m_dbStencilControl.bits.STENCILZPASS_BF = HwStencilOp(dsState.back.stencilPassOp);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* DepthStencilState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_CONTROL,   m_dbDepthControl.u32All,   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_CONTROL, m_dbStencilControl.u32All, pCmdSpace);

    return pCmdSpace;
}

// =====================================================================================================================
// Converts a Pal::StencilOp enum value to a Gfx9 hardware StencilOp enum.
Gfx9::StencilOp DepthStencilState::HwStencilOp(
    Pal::StencilOp stencilOp)
{
    constexpr Gfx9::StencilOp StencilOpTbl[] =
    {
        STENCIL_KEEP,         // Keep
        STENCIL_ZERO,         // Zero
        STENCIL_REPLACE_TEST, // Replace
        STENCIL_ADD_CLAMP,    // IncClamp
        STENCIL_SUB_CLAMP,    // DecClamp
        STENCIL_INVERT,       // Invert
        STENCIL_ADD_WRAP,     // IncWrap
        STENCIL_SUB_WRAP      // DecWrap
    };

    return StencilOpTbl[static_cast<int32>(stencilOp)];
}

// =====================================================================================================================
// Converts a Pal::CompareFunc enum value to a Gfx9 hardware CompareFrag enum.
CompareFrag DepthStencilState::HwDepthCompare(
    CompareFunc func)
{
    constexpr CompareFrag DepthCompareTbl[] =
    {
        FRAG_NEVER,     // Never
        FRAG_LESS,      // Less
        FRAG_EQUAL,     // Equal
        FRAG_LEQUAL,    // LessEqual
        FRAG_GREATER,   // Greater
        FRAG_NOTEQUAL,  // NotEqual
        FRAG_GEQUAL,    // GreaterEqual
        FRAG_ALWAYS,    // Always
    };

    return DepthCompareTbl[static_cast<int32>(func)];
}

// =====================================================================================================================
// Converts a Pal::CompareFunc enum value to a Gfx9 hardware CompareRef enum.
CompareRef DepthStencilState::HwStencilCompare(
    CompareFunc func)
{
    constexpr CompareRef StencilCompareTbl[] =
    {
        REF_NEVER,     // Never
        REF_LESS,      // Less
        REF_EQUAL,     // Equal
        REF_LEQUAL,    // LessEqual
        REF_GREATER,   // Greater
        REF_NOTEQUAL,  // NotEqual
        REF_GEQUAL,    // GreaterEqual
        REF_ALWAYS,    // Always
    };

    return StencilCompareTbl[static_cast<int32>(func)];
}

} // Gfx9
} // Pal
