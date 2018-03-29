/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx9/gfx9Chip.h"

namespace Pal
{

struct GraphicsState;

namespace Gfx9
{

class  CmdStream;
class  CmdUtil;
class  Device;
struct Gfx9PalSettings;
class  UniversalCmdBuffer;
struct UniversalCmdBufferState;

// =====================================================================================================================
// Maintains state for hardware workarounds which need tracking of changes between draws. (NOTE - this tracking is not
// limited to things like bound objects, but can also include number of vertices per draw, etc.).
// It is intended for these objects to be owned by Universal Command Buffers.
class WorkaroundState
{
public:
    WorkaroundState(
        const Device*                  pDevice,
        bool                           isNested,
        const UniversalCmdBufferState& universalState);
    ~WorkaroundState() {}

    template <bool indirect, bool stateDirty, bool pm4OptImmediate>
    uint32* PreDraw(
        const GraphicsState&    gfxState,
        CmdStream*              pDeCmdStream,
        UniversalCmdBuffer*     pCmdBuffer,
        const ValidateDrawInfo& drawInfo,
        uint32*                 pCmdSpace);

    uint32* SwitchToNggPipeline(
        bool                    isFirstDraw,
        bool                    prevPipelineIsNgg,
        bool                    prevPipelineUsesTess,
        bool                    prevPipelineUsesGs,
        bool                    usesOffchipPc,
        CmdStream*              pDeCmdStream,
        uint32*                 pCmdSpace) const;

private:
    const Device&                  m_device;
    const CmdUtil&                 m_cmdUtil;
    const Gfx9PalSettings&         m_settings;
    const bool                     m_isNested;
    const UniversalCmdBufferState& m_universalState;

    PAL_DISALLOW_DEFAULT_CTOR(WorkaroundState);
    PAL_DISALLOW_COPY_AND_ASSIGN(WorkaroundState);
};

} // Gfx9
} // Pal
