/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/queueContext.h"
#include "core/hw/gfxip/gfx9/gfx9CmdStream.h"
#include "core/hw/gfxip/gfx9/gfx9ShadowedRegisters.h"

namespace Pal
{
namespace Gfx9
{

class ComputeEngine;
class Device;
class UniversalEngine;

// =====================================================================================================================
class ComputeQueueContext : public QueueContext
{
public:
    ComputeQueueContext(Device* pDevice, Queue* pQueue, Engine* pEngine, uint32 queueId);
    virtual ~ComputeQueueContext() { }

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, const SubmitInfo& submitInfo) override;
    virtual void PostProcessSubmit() override;

private:
    Result RebuildCommandStreams();

    Device*const        m_pDevice;
    Queue*const         m_pQueue;
    ComputeEngine*const m_pEngine;
    uint32              m_queueId;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;

    CmdStream  m_cmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_postambleCmdStream;

    PAL_DISALLOW_DEFAULT_CTOR(ComputeQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeQueueContext);
};

// =====================================================================================================================
class UniversalQueueContext : public QueueContext
{
public:
    UniversalQueueContext(Device* pDevice, Queue* pQueue, Engine* pEngine, uint32 queueId);
    virtual ~UniversalQueueContext();

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, const SubmitInfo& submitInfo) override;
    virtual void PostProcessSubmit() override;
    virtual Result ProcessInitialSubmit(InternalSubmitInfo* pSubmitInfo) override;

private:
    Result BuildShadowPreamble();
    Result RebuildCommandStreams();

    Result AllocateShadowMemory();

    void WritePerSubmitPreamble(CmdStream* pCmdStream, bool initShadowMemory);
    uint32* WriteUniversalPreamble(uint32* pCmdSpace);

    Device*const          m_pDevice;
    Queue*const           m_pQueue;
    UniversalEngine*const m_pEngine;
    uint32                m_queueId;

    // Current watermark for the device-initiated context updates which have been processed by this queue context.
    uint32  m_currentUpdateCounter;

    // GPU memory allocation used for shadowing persistent CE RAM between submissions.
    bool            m_useShadowing;
    BoundGpuMemory  m_shadowGpuMem;
    gpusize         m_shadowGpuMemSizeInBytes;
    uint32          m_shadowedRegCount; // Number of state registers shadowed using state-shadowing.

    // Command streams which restore hardware to a known state before launching command buffers.
    CmdStream  m_deCmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_shadowInitCmdStream;
    CmdStream  m_cePreambleCmdStream;
    CmdStream  m_cePostambleCmdStream;
    CmdStream  m_dePostambleCmdStream;

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

} // Gfx9
} // Pal
