/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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
DepthStencilState::DepthStencilState(
    const DepthStencilStateCreateInfo& createInfo)
    :
    Pal::DepthStencilState(createInfo)
{
    memset(&m_flags, 0, sizeof(m_flags));

    m_flags.isDepthEnabled   = createInfo.depthEnable;
    m_flags.isStencilEnabled = createInfo.stencilEnable;

    m_flags.isDepthWriteEnabled =
        createInfo.depthEnable      &&
        createInfo.depthWriteEnable &&
        (createInfo.depthFunc != CompareFunc::Never);

    m_flags.isStencilWriteEnabled =
        createInfo.stencilEnable           &&
        ((createInfo.front.stencilFailOp != Pal::StencilOp::Keep)      ||
         (createInfo.front.stencilPassOp != Pal::StencilOp::Keep)      ||
         (createInfo.front.stencilDepthFailOp != Pal::StencilOp::Keep) ||
         (createInfo.back.stencilFailOp != Pal::StencilOp::Keep)       ||
         (createInfo.back.stencilPassOp != Pal::StencilOp::Keep)       ||
         (createInfo.back.stencilDepthFailOp != Pal::StencilOp::Keep));

    m_flags.canDepthRunOutOfOrder =
        (createInfo.depthEnable == false)      ||
        (m_flags.isDepthWriteEnabled == false) ||
        CanRunOutOfOrder(createInfo.depthFunc);

    m_flags.canStencilRunOutOfOrder =
        (createInfo.stencilEnable == false)          ||
        (m_flags.isStencilWriteEnabled == false)     ||
        (CanRunOutOfOrder(createInfo.front.stencilFunc) &&
         CanRunOutOfOrder(createInfo.back.stencilFunc));

    m_flags.depthForcesOrdering =
        createInfo.depthEnable                        &&
        (createInfo.depthFunc != CompareFunc::Always) &&
        (createInfo.depthFunc != CompareFunc::NotEqual);

    m_flags.isDepthBoundsEnabled = createInfo.depthBoundsEnable;
}

// =====================================================================================================================
DB_DEPTH_CONTROL DepthStencilState::SetupDbDepthControl(
    const DepthStencilStateCreateInfo& createInfo)
{
    DB_DEPTH_CONTROL dbDepthControl;
    dbDepthControl.u32All = 0;

    dbDepthControl.bits.Z_ENABLE       = (createInfo.depthEnable ? 1 : 0);
    dbDepthControl.bits.Z_WRITE_ENABLE = (createInfo.depthWriteEnable ? 1 : 0);
    dbDepthControl.bits.ZFUNC          = HwDepthCompare(createInfo.depthFunc);

    dbDepthControl.bits.STENCIL_ENABLE = (createInfo.stencilEnable ? 1 : 0);
    dbDepthControl.bits.STENCILFUNC    = HwStencilCompare(createInfo.front.stencilFunc);
    dbDepthControl.bits.STENCILFUNC_BF = HwStencilCompare(createInfo.back.stencilFunc);

    dbDepthControl.bits.DEPTH_BOUNDS_ENABLE = (createInfo.depthBoundsEnable ? 1 : 0);
    // NOTE: Always on
    dbDepthControl.bits.BACKFACE_ENABLE = 1;

    // Force off as this is not linked to any API features. Their need/use is unclear.
    dbDepthControl.bits.ENABLE_COLOR_WRITES_ON_DEPTH_FAIL  = 0;
    dbDepthControl.bits.DISABLE_COLOR_WRITES_ON_DEPTH_PASS = 0;

    return dbDepthControl;
}

// =====================================================================================================================
DB_STENCIL_CONTROL DepthStencilState::SetupDbStencilControl(
    const DepthStencilStateCreateInfo& createInfo)
{
    DB_STENCIL_CONTROL dbStencilControl;
    dbStencilControl.u32All = 0;

    // front stencil
    dbStencilControl.bits.STENCILFAIL  = HwStencilOp(createInfo.front.stencilFailOp);
    dbStencilControl.bits.STENCILZFAIL = HwStencilOp(createInfo.front.stencilDepthFailOp);
    dbStencilControl.bits.STENCILZPASS = HwStencilOp(createInfo.front.stencilPassOp);

    // back stencil
    dbStencilControl.bits.STENCILFAIL_BF  = HwStencilOp(createInfo.back.stencilFailOp);
    dbStencilControl.bits.STENCILZFAIL_BF = HwStencilOp(createInfo.back.stencilDepthFailOp);
    dbStencilControl.bits.STENCILZPASS_BF = HwStencilOp(createInfo.back.stencilPassOp);

    return dbStencilControl;
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

// =====================================================================================================================
Gfx11DepthStencilStateRs64::Gfx11DepthStencilStateRs64(
    const DepthStencilStateCreateInfo& createInfo)
    :
    Gfx9::DepthStencilState(createInfo)
{
    m_regs.offset0 = mmDB_DEPTH_CONTROL - CONTEXT_SPACE_START;
    m_regs.value0  = SetupDbDepthControl(createInfo).u32All;

    m_regs.offset1 = mmDB_STENCIL_CONTROL - CONTEXT_SPACE_START;
    m_regs.value1  = SetupDbStencilControl(createInfo).u32All;
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx11DepthStencilStateRs64::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WriteSetConstContextRegPairs(&m_regs, 2, pCmdSpace);
}

// =====================================================================================================================
Gfx11DepthStencilStateF32::Gfx11DepthStencilStateF32(
    const DepthStencilStateCreateInfo& createInfo)
    :
    Gfx9::DepthStencilState(createInfo)
{
    // Initialize structure (reg offsets).
    Regs::Init(m_regs);

    // Setup DB_DEPTH_CONTROL.
    *(Regs::Get<mmDB_DEPTH_CONTROL, DB_DEPTH_CONTROL>(m_regs)) = SetupDbDepthControl(createInfo);

    // Setup DB_STENCIL_CONTROL.
    *(Regs::Get<mmDB_STENCIL_CONTROL, DB_STENCIL_CONTROL>(m_regs)) = SetupDbStencilControl(createInfo);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx11DepthStencilStateF32::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    return pCmdStream->WriteSetContextRegPairs(m_regs, Regs::Size(), pCmdSpace);
}

// =====================================================================================================================
Gfx10DepthStencilState::Gfx10DepthStencilState(
    const DepthStencilStateCreateInfo& createInfo)
    :
    Gfx9::DepthStencilState(createInfo)
{
    // Setup DB_DEPTH_CONTROL.
    m_dbDepthControl = SetupDbDepthControl(createInfo);

    // Setup DB_STENCIL_CONTROL.
    m_dbStencilControl = SetupDbStencilControl(createInfo);
}

// =====================================================================================================================
// Writes the PM4 commands required to bind the state object to the specified bind point. Returns the next unused DWORD
// in pCmdSpace.
uint32* Gfx10DepthStencilState::WriteCommands(
    CmdStream* pCmdStream,
    uint32*    pCmdSpace
    ) const
{
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_DEPTH_CONTROL,   m_dbDepthControl.u32All,   pCmdSpace);
    pCmdSpace = pCmdStream->WriteSetOneContextReg(mmDB_STENCIL_CONTROL, m_dbStencilControl.u32All, pCmdSpace);

    return pCmdSpace;
}

} // Gfx9
} // Pal
