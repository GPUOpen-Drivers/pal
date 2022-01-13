/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{
namespace Gfx6
{

class Device;

// =====================================================================================================================
// Gfx6 hardware layer DepthStencil State class: implements GCN specific functionality for the ApiStateObject class,
// specifically for depth/stencil state.
class DepthStencilState final : public Pal::DepthStencilState
{
public:
    explicit DepthStencilState(const DepthStencilStateCreateInfo& createInfo);

    static CompareRef HwStencilCompare(CompareFunc func);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    bool IsDepthEnabled() const { return (m_flags.isDepthEnabled != 0); }
    bool IsDepthWriteEnabled() const { return (m_flags.isDepthWriteEnabled != 0); }
    bool IsStencilWriteEnabled() const { return (m_flags.isStencilWriteEnabled != 0); }

    bool CanDepthRunOutOfOrder() const { return (m_flags.canDepthRunOutOfOrder != 0); }
    bool CanStencilRunOutOfOrder() const { return (m_flags.canStencilRunOutOfOrder != 0); }
    bool DepthForcesOrdering() const { return (m_flags.depthForcesOrdering != 0); }

private:
    virtual ~DepthStencilState() { }

    void Init(const DepthStencilStateCreateInfo& dsState);

    static CompareFrag HwDepthCompare(CompareFunc func);
    static Gfx6::StencilOp HwStencilOp(Pal::StencilOp stencilOp);

    union
    {
        struct
        {
            uint32 isDepthEnabled          : 1;
            uint32 isDepthWriteEnabled     : 1;
            uint32 isStencilWriteEnabled   : 1;
            uint32 canDepthRunOutOfOrder   : 1;  // Indicates depth buffer will have the same result regardless of the
                                                 // order in which geometry is Z tested.
            uint32 canStencilRunOutOfOrder : 1;  // Indicates stencil buffer will have the same result regardless of
                                                 // the order in which geometry is S tested.
            uint32 depthForcesOrdering     : 1;  // Indicates depth test will force the geometry to be ordered in a
                                                 // predictable way.
            uint32 reserved                : 26;
        };
        uint32 u32All;
    } m_flags;

    regDB_DEPTH_CONTROL    m_dbDepthControl;
    regDB_STENCIL_CONTROL  m_dbStencilControl;

    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilState);
};

} // Gfx6
} // Pal
