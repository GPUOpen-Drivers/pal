/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

#if PAL_BUILD_CMD_BUFFER_LOGGER

#include "core/layers/decorators.h"

namespace Pal
{
namespace CmdBufferLogger
{

class CmdBuffer;
class Device;

// =====================================================================================================================
class Queue : public QueueDecorator
{
public:
    Queue(IQueue*    pNextQueue,
          Device*    pDevice);

    Result Init(
        EngineType engineType,
        QueueType  queueType);

    // Public IQueue interface methods:
    virtual Result Submit(
        const MultiSubmitInfo& submitInfo) override;
    virtual void Destroy() override;

private:
    virtual ~Queue() {}

    Result InitCmdBuffer(EngineType engineType, QueueType  queueType);
    void AddRemapRange(
        VirtualMemoryRemapRange* pRange,
        CmdBuffer*               pCmdBuffer) const;

    Device*const   m_pDevice;
    bool           m_timestampingActive;
    ICmdAllocator* m_pCmdAllocator;
    CmdBuffer*     m_pCmdBuffer;
    IGpuMemory*    m_pTimestamp;
    IFence*        m_pFence;

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // CmdBufferLogger
} // Pal

#endif
