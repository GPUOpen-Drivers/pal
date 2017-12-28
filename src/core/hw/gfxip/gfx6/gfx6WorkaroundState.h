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

#include "core/hw/gfxip/gfx6/gfx6Chip.h"

namespace Pal
{

struct GraphicsState;

namespace Gfx6
{

class CmdUtil;
class Device;

// =====================================================================================================================
// Maintains state for hardware workarounds which need tracking of changes between draws. (NOTE - this tracking is not
// limited to things like bound objects, but can also include number of vertices per draw, etc.).
// It is intended for these objects to be owned by Universal Command Buffers.
class WorkaroundState
{
public:
    explicit WorkaroundState(const Device* pDevice, bool isNested);
    ~WorkaroundState() {}

    template <bool indirect, bool stateDirty>
    uint32* PreDraw(const GraphicsState&    gfxState,
                    CmdStream&              deCmdStream,
                    regIA_MULTI_VGT_PARAM   iaMultiVgtParam,
                    const ValidateDrawInfo& drawInfo,
                    uint32*                 pCmdSpace);

    uint32* PostDraw(const GraphicsState& gfxState,
                     uint32*              pCmdSpace);

    void Reset();

    void LeakNestedCmdBufferState(const WorkaroundState& workaroundState);

    // Clears CB_COLOR0_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE bit for given slot.
    void ClearDccOverwriteCombinerDisable(uint32 slot) { m_dccOverwriteCombinerDisableMask &= ~(1 << slot); }

private:
    const Device&          m_device;
    const CmdUtil&         m_cmdUtil;
    const Gfx6PalSettings& m_settings;
    const bool             m_isNested;

    // Mask for CB_COLOR0_DCC_CONTROL.OVERWRITE_COMBINER_DISABLE bit per target.
    uint32  m_dccOverwriteCombinerDisableMask;

    // Previously validated primitive restart value's index type. Only Vulkan clients need to track this state.
    IndexType  m_multiPrimRestartIndexType;

    PAL_DISALLOW_DEFAULT_CTOR(WorkaroundState);
    PAL_DISALLOW_COPY_AND_ASSIGN(WorkaroundState);
};

} // Gfx6
} // Pal
