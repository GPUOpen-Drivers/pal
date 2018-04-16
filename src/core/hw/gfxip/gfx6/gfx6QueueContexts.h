/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "core/hw/gfxip/gfx6/gfx6CmdStream.h"
#include "core/hw/gfxip/gfx6/gfx6Preambles.h"

namespace Pal
{
namespace Gfx6
{

class ComputeEngine;
class Device;
class UniversalEngine;

// =====================================================================================================================
// GFX6+ hardware requires an internal scratch ring of memory to be used for register spilling if a shader uses too many
// temp registers. This scratch ring can be dynamically resized based on the highest scratch memory needs of any Compute
// Pipeline which has been created thus far. Thus, a small command stream needs to be submitted along with any client
// submission which follows a resize event or context-switch between applications. This class is responsible for
// guaranteeing that the scratch ring is in a valid state before launching GPU work.
//
// SEE: Gfx6::ShaderRingMgr, ShaderRingSet and ShaderRing.
class ComputeQueueContext : public QueueContext
{
public:
    ComputeQueueContext(Device* pDevice, Queue* pQueue, Engine* pEngine, uint32 queueId);
    virtual ~ComputeQueueContext() {}

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, const SubmitInfo& submitInfo) override;
    virtual void PostProcessSubmit() override;

private:
    void RebuildCommandStream();

    void BuildComputePreambleHeaders();
    void SetupComputePreambleRegisters();

    Device*const        m_pDevice;
    Queue*const         m_pQueue;
    ComputeEngine*const m_pEngine;
    uint32              m_queueId;
    uint32              m_currentUpdateCounter;  // Current watermark for the device-initiated context updates that
                                                 // have been processed by this queue context.

    // Command stream which restores hardware to a known state before launching command buffers.
    CmdStream           m_cmdStream;
    CmdStream           m_perSubmitCmdStream;
    CmdStream           m_postambleCmdStream;

    CommonPreamblePm4Img  m_commonPreamble;  // Image of PM4 commands for common state.
    ComputePreamblePm4Img m_computePreamble; // Image of PM4 commands for compute-only state.

    PAL_DISALLOW_DEFAULT_CTOR(ComputeQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(ComputeQueueContext);
};

// =====================================================================================================================
// In addition to the internal scratch ring requirement mentioned above, GFX6+ hardware also requires several internal
// memory rings for various other needs (such as Geometry Shaders or Tessellation). Like with the scratch rings, these
// others can also be dynamically resized based on the highest ring memory needs of any Compute or Graphics Pipeline
// which has been created thus far. Furthermore, some hardware has a bug which doesn't restore the state of certain
// registers after a power-managment event. Thus, a pair of small command streams may need to be submitted along with
// any client submission which follows a resize event, power-managment event, or context-switch between applications.
// This class is responsible for guaranteeing that the state of the internal memory rings and non-restored registers
// are valid before launching GPU work.
//
// SEE: Gfx6::ShaderRingMgr, ShaderRingSet and ShaderRing.
class UniversalQueueContext : public QueueContext
{
public:
    UniversalQueueContext(Device* pDevice, Queue* pQueue, Engine* pEngine, uint32 queueId);
    virtual ~UniversalQueueContext();

    Result Init();

    virtual Result PreProcessSubmit(InternalSubmitInfo* pSubmitInfo, const SubmitInfo& submitInfo) override;
    virtual void PostProcessSubmit() override;

private:
    void RebuildCommandStreams();
    Result AllocateShadowMemory();

    void BuildUniversalPreambleHeaders();
    void SetupUniversalPreambleRegisters();

    Device*const          m_pDevice;
    Queue*const           m_pQueue;
    UniversalEngine*const m_pEngine;
    uint32                m_queueId;

    uint32           m_currentUpdateCounter;  // Current watermark for the device-initiated context updates that have
                                              // been processed by this queue context.

    // GPU memory allocation used for shadowing persistent CE RAM between submissions.
    bool            m_useShadowing;
    BoundGpuMemory  m_shadowGpuMem;
    gpusize         m_shadowGpuMemSizeInBytes;
    uint32          m_shadowedRegCount; // Number of state registers shadowed using state-shadowing.
    uint32          m_submitCounter;    // How many times have we submitted. For the first submit, we need to include
                                        // the set packets in m_perSubmitCmdStream after the load packets to initialize
                                        // the register and shadow memory. On the second submit, we need to rebuild the
                                        // command stream because the set should only be done once.

    // Command streams which restore hardware to a known state before launching command buffers.
    CmdStream  m_deCmdStream;
    CmdStream  m_perSubmitCmdStream;
    CmdStream  m_cePreambleCmdStream;
    CmdStream  m_cePostambleCmdStream;
    CmdStream  m_dePostambleCmdStream;

    CommonPreamblePm4Img              m_commonPreamble;             // Image of PM4 commands for common state.
    UniversalPreamblePm4Img           m_universalPreamble;          // Image of PM4 commands for universal-only state.
    Gfx6UniversalPreamblePm4Img       m_gfx6UniversalPreamble;      // Image of PM4 commands for Gfx6-specific state.
    Gfx8UniversalPreamblePm4Img       m_gfx8UniversalPreamble;      // Image of PM4 commands for Gfx8-specific state.
    StateShadowPreamblePm4Img         m_stateShadowPreamble;        // Image of PM4 commands for GPU state-shadowing.

    PAL_DISALLOW_DEFAULT_CTOR(UniversalQueueContext);
    PAL_DISALLOW_COPY_AND_ASSIGN(UniversalQueueContext);
};

} // Gfx6
} // Pal
