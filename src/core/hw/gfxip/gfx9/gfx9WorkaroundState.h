/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

namespace Pm4
{
struct GraphicsState;
}

namespace Gfx9
{

class  CmdStream;
class  CmdUtil;
class  Device;
class  GraphicsPipeline;
class  UniversalCmdBuffer;
struct UniversalCmdBufferState;
union  CachedSettings;

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
        const UniversalCmdBufferState& universalState,
        const CachedSettings&          cachedSettings);
    ~WorkaroundState() {}

    template <bool PipelineDirty, bool StateDirty, bool Pm4OptImmediate>
    uint32* PreDraw(
        const Pm4::GraphicsState& gfxState,
        CmdStream*                pDeCmdStream,
        UniversalCmdBuffer*       pCmdBuffer,
        uint32*                   pCmdSpace);
    uint32* SwitchToLegacyPipeline(
        bool                    oldPipelineUsesGs,
        bool                    oldPipelineNgg,
        uint32                  oldCutMode,
        bool                    oldPipelineUnknown,
        const GraphicsPipeline* pNewPipeline,
        uint32*                 pCmdSpace
        ) const;
    void HandleZeroIndexBuffer(
        UniversalCmdBuffer* pCmdBuffer,
        gpusize*            pIndexBufferAddr,
        uint32*             pIndexCount);
    template <bool Indirect>
    bool DisableInstancePacking(
        PrimitiveTopology   topology,
        uint32              instanceCount,
        uint32              numActiveQueries) const;

    regDB_RENDER_CONTROL SetOreoMode(
        regDB_RENDER_CONTROL    dbRenderControl,
        const GraphicsPipeline* pPipeline) const;

private:
    const Device&                  m_device;
    const CmdUtil&                 m_cmdUtil;
    const CachedSettings&          m_cachedSettings;
    const bool                     m_isNested;
    const UniversalCmdBufferState& m_universalState;

    PAL_DISALLOW_DEFAULT_CTOR(WorkaroundState);
    PAL_DISALLOW_COPY_AND_ASSIGN(WorkaroundState);
};

} // Gfx9
} // Pal
