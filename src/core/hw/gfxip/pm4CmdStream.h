/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/hw/gfxip/gfxCmdStream.h"
#include "core/hw/gfxip/gfxDevice.h"
#include "palVector.h"

namespace Pal
{

class GfxDevice;
enum  QueueType : uint32;

namespace Pm4
{

// =====================================================================================================================
// Implements control flow and other code common to GFX-specific command stream implementations.
class CmdStream : public GfxCmdStream
{
public:
    virtual ~CmdStream() { }

    virtual void Call(const Pal::CmdStream& targetStream, bool exclusiveSubmit, bool allowIb2Launch) override;

    void ExecuteGeneratedCommands(CmdStreamChunk** ppChunkList, uint32 numChunksExecuted, uint32 numGenChunks);

    uint32 PrepareChunkForCmdGeneration(
        CmdStreamChunk* pChunk,
        uint32          cmdBufStride,           // In dwords
        uint32          embeddedDataStride,     // In dwords
        uint32          maxCommands) const;

protected:
    CmdStream(
        const GfxDevice& device,
        ICmdAllocator*   pCmdAllocator,
        EngineType       engineType,
        SubEngineType    subEngineType,
        CmdStreamUsage   cmdStreamUsage,
        uint32           chainSizeInDwords,
        uint32           minNopSizeInDwords,
        uint32           condIndirectBufferSize,
        bool             isNested);

    PAL_DISALLOW_COPY_AND_ASSIGN(CmdStream);
    PAL_DISALLOW_DEFAULT_CTOR(CmdStream);
};

}; // Pm4
}; // Pal
