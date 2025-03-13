/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/device.h"
#include "core/hw/gfxip/gfx12/gfx12CmdStream.h"
#include "core/hw/gfxip/gfx12/gfx12DepthStencilState.h"
#include "palInlineFuncs.h"

using namespace Util;
using namespace Pal::Gfx12::Chip;

namespace Pal
{
namespace Gfx12
{

// =====================================================================================================================
// Converts a Pal::StencilOp enum value to a Gfx12 hardware StencilOp enum.
static StencilOp HwStencilOp(
    Pal::StencilOp stencilOp)
{
    constexpr StencilOp StencilOpTbl[] =
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
// Converts a Pal::CompareFunc enum value to a Gfx12 hardware CompareFrag enum.
static CompareFrag HwDepthCompare(
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
// Converts a Pal::CompareFunc enum value to a Gfx12 hardware CompareRef enum.
static CompareRef HwStencilCompare(
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
DepthStencilState::DepthStencilState(
    const Device&                      device,
    const DepthStencilStateCreateInfo& createInfo)
    :
    Pal::DepthStencilState(createInfo),
    m_regs{}
{
    m_regs.dbDepthControl.bits.Z_ENABLE            = createInfo.depthEnable;
    m_regs.dbDepthControl.bits.Z_WRITE_ENABLE      = createInfo.depthWriteEnable;
    m_regs.dbDepthControl.bits.ZFUNC               = HwDepthCompare(createInfo.depthFunc);
    m_regs.dbDepthControl.bits.DEPTH_BOUNDS_ENABLE = createInfo.depthBoundsEnable;
    m_regs.dbDepthControl.bits.STENCIL_ENABLE      = createInfo.stencilEnable;
    m_regs.dbDepthControl.bits.STENCILFUNC         = HwStencilCompare(createInfo.front.stencilFunc);
    m_regs.dbDepthControl.bits.BACKFACE_ENABLE     = 1;
    m_regs.dbDepthControl.bits.STENCILFUNC_BF      = HwStencilCompare(createInfo.back.stencilFunc);

    m_regs.dbStencilControl.bits.STENCILFAIL      = HwStencilOp(createInfo.front.stencilFailOp);
    m_regs.dbStencilControl.bits.STENCILZFAIL     = HwStencilOp(createInfo.front.stencilDepthFailOp);
    m_regs.dbStencilControl.bits.STENCILZPASS     = HwStencilOp(createInfo.front.stencilPassOp);
    m_regs.dbStencilControl.bits.STENCILFAIL_BF   = HwStencilOp(createInfo.back.stencilFailOp);
    m_regs.dbStencilControl.bits.STENCILZFAIL_BF  = HwStencilOp(createInfo.back.stencilDepthFailOp);
    m_regs.dbStencilControl.bits.STENCILZPASS_BF  = HwStencilOp(createInfo.back.stencilPassOp);
}

// =====================================================================================================================
uint32* DepthStencilState::WriteCommands(
    uint32* pCmdSpace
    ) const
{
    static_assert(Util::CheckSequential({ mmDB_DEPTH_CONTROL,
                                          mmDB_STENCIL_CONTROL, }),
                  "DepthStencilState registers are not sequential!");
    static_assert(Util::CheckSequential({ offsetof(DepthStencilStateRegs, dbDepthControl),
                                          offsetof(DepthStencilStateRegs, dbStencilControl), },
                                        sizeof(uint32)),
                  "Storage order of DepthStencilStateRegs is important!");

    return CmdStream::WriteSetSeqContextRegs(mmDB_DEPTH_CONTROL,
                                             mmDB_STENCIL_CONTROL,
                                             &(m_regs.dbDepthControl.u32All),
                                             pCmdSpace);
}

} // namespace Gfx12
} // namespace Pal
