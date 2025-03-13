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

#pragma once

#include "core/hw/gfxip/depthStencilState.h"
#include "core/hw/gfxip/gfxCmdBuffer.h"
#include "core/hw/gfxip/gfx12/gfx12RegPairHandler.h"

namespace Pal
{
namespace Gfx12
{

class Device;

// =====================================================================================================================
// Gfx12 DepthStencilState object.  Translates PAL interface depth/stencil controls to Gfx12.  Hardware independent.
class DepthStencilState final : public Pal::DepthStencilState
{
public:
    DepthStencilState(const Device& device, const DepthStencilStateCreateInfo& createInfo);

    uint32* WriteCommands(uint32* pCmdSpace) const;

    DB_DEPTH_CONTROL   DbDepthControl()   const { return m_regs.dbDepthControl;   }
    DB_STENCIL_CONTROL DbStencilControl() const { return m_regs.dbStencilControl; }

private:
    virtual ~DepthStencilState() { }

    struct DepthStencilStateRegs
    {
        DB_DEPTH_CONTROL   dbDepthControl;
        DB_STENCIL_CONTROL dbStencilControl;
    };

    DepthStencilStateRegs m_regs;

    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilState);
    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilState);
};

} // namespace Gfx12
} // namespace Pal
