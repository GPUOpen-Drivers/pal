/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"
#include "core/hw/gfxip/gfx9/gfx9Device.h"

namespace Pal
{
namespace Gfx9
{

class Device;

// =====================================================================================================================
// Represents an "image" of the PM4 commands necessary to write a DepthStencilState to hardware.
// The required register writes are grouped into sets based on sequential register addresses, so that we can minimize
// the amount of PM4 space needed by setting several reg's in each packet.
struct DepthStencilStatePm4Img
{
    PM4_PFP_SET_CONTEXT_REG     hdrDepthControl;    // 1st PM4 set context reg packet
    regDB_DEPTH_CONTROL         dbDepthControl;     // Controls depth/stencil test.

    PM4_PFP_SET_CONTEXT_REG     hdrStencilControl;  // 2nd PM4 set context reg packet
    regDB_STENCIL_CONTROL       dbStencilControl;   // More controls for stencil test.
};

// =====================================================================================================================
// Gfx9 hardware layer DepthStencil State class: implements Gfx9 specific functionality for the IDepthStencilState
// class.
class DepthStencilState : public Pal::DepthStencilState
{
public:
    DepthStencilState(const Device& device);
    Result Init(const DepthStencilStateCreateInfo& dsState);

    static size_t Pm4ImgSize() { return sizeof(DepthStencilStatePm4Img); }
    static CompareRef  HwStencilCompare(CompareFunc func);

    uint32* WriteCommands(CmdStream* pCmdStream, uint32* pCmdSpace) const;

    bool IsDepthEnabled() const { return (m_flags.isDepthEnabled != 0); }
    bool IsStencilEnabled() const { return (m_flags.isStencilEnabled != 0); }
    bool IsDepthWriteEnabled() const { return (m_flags.isDepthWriteEnabled != 0); }
    bool IsStencilWriteEnabled() const { return (m_flags.isStencilWriteEnabled != 0); }
    bool CanDepthRunOutOfOrder() const { return (m_flags.canDepthRunOutOfOrder != 0); }
    bool CanStencilRunOutOfOrder() const { return (m_flags.canStencilRunOutOfOrder != 0); }
    bool DepthForcesOrdering() const { return (m_flags.depthForcesOrdering != 0); }

protected:
    virtual ~DepthStencilState() {}

private:
    void BuildPm4Headers();

    static CompareFrag HwDepthCompare(CompareFunc func);
    static ::StencilOp HwStencilOp(StencilOp stencilOp);

    const Device&            m_device;

    // Image of PM4 commands needed to write this object to hardware.
    DepthStencilStatePm4Img  m_pm4Commands;

    struct
    {
        uint32 isDepthEnabled          : 1;
        uint32 isStencilEnabled        : 1;
        uint32 isDepthWriteEnabled     : 1;
        uint32 isStencilWriteEnabled   : 1;
        uint32 canDepthRunOutOfOrder   : 1;  // Indicates depth buffer will have the same result regardless of the order
                                             // in which geometry is Z tested.
        uint32 canStencilRunOutOfOrder : 1;  // Indicates stencil buffer will have the same result regardless of the
                                             // order in which geometry is S tested.
        uint32 depthForcesOrdering     : 1;  // Indicates depth test will force the geometry to be ordered in a
                                             // predictable way.
        uint32 reserved                : 25;
    } m_flags;

    PAL_DISALLOW_COPY_AND_ASSIGN(DepthStencilState);
    PAL_DISALLOW_DEFAULT_CTOR(DepthStencilState);
};

} // Gfx9
} // Pal
