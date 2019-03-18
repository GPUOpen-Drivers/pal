/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018-2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "palCmdBuffer.h"
#include "palDeque.h"
#include "palLinearAllocator.h"
#include "core/layers/decorators.h"

enum Sdl_HwShaderStage;

namespace Pal
{
namespace ShaderDbg
{

class Device;
class Pipeline;
class Platform;

// Contains all of the critical pieces of information to identify a particular draw in this command buffer
// so that it can be dumped to disk after it has been submitted.
struct TraceData
{
    const Pipeline*   pPipeline;
    IGpuMemory*       pTraceMemory;
    Sdl_HwShaderStage hwStage;
    bool              isDraw;
    uint32            uniqueId;
    uint64            apiPsoHash;
};

// =====================================================================================================================
// ShaderDbg implementation of the ICmdBuffer interface.  In addition to passing commands on to the next layer,
// any draw or dispatch that contains an instrumented shader will allocate a buffer for the shader to log data into.
class CmdBuffer: public CmdBufferFwdDecorator
{
public:
    CmdBuffer(ICmdBuffer*                pNextCmdBuffer,
              Device*                    pDevice,
              const CmdBufferCreateInfo& createInfo);

    // Public ICmdBuffer interface methods.  Each one tokenizes the call and returns immediately.
    virtual Result Begin(
        const CmdBufferBuildInfo& info) override;
    virtual Result End() override;
    virtual Result Reset(ICmdAllocator* pCmdAllocator, bool returnGpuMemory) override;
    virtual void CmdBindPipeline(
        const PipelineBindParams& params) override;

    const Util::Deque<TraceData, Platform>& GetTraceData() const { return m_traceData; }

    // Part of the IDestroyable public interface.
    virtual void Destroy() override
    {
        ICmdBuffer* pNextLayer = m_pNextLayer;
        this->~CmdBuffer();
        pNextLayer->Destroy();
    }

private:
    virtual ~CmdBuffer();

    Result ResetState();

    static void PAL_STDCALL CmdSetUserDataCs(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);
    static void PAL_STDCALL CmdSetUserDataGfx(
        ICmdBuffer*   pCmdBuffer,
        uint32        firstEntry,
        uint32        entryCount,
        const uint32* pEntryValues);
    static void PAL_STDCALL CmdDraw(
        ICmdBuffer* pCmdBuffer,
        uint32      firstVertex,
        uint32      vertexCount,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawOpaque(
        ICmdBuffer* pCmdBuffer,
        gpusize     streamOutFilledSizeVa,
        uint32      streamOutOffset,
        uint32      stride,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawIndexed(
        ICmdBuffer* pCmdBuffer,
        uint32      firstIndex,
        uint32      indexCount,
        int32       vertexOffset,
        uint32      firstInstance,
        uint32      instanceCount);
    static void PAL_STDCALL CmdDrawIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDrawIndexedIndirectMulti(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDispatch(
        ICmdBuffer* pCmdBuffer,
        uint32      x,
        uint32      y,
        uint32      z);
    static void PAL_STDCALL CmdDispatchIndirect(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    static void PAL_STDCALL CmdDispatchOffset(
        ICmdBuffer* pCmdBuffer,
        uint32      xOffset,
        uint32      yOffset,
        uint32      zOffset,
        uint32      xDim,
        uint32      yDim,
        uint32      zDim);

    void AllocateHwShaderDbg(
        bool   isDraw,
        uint32 uniqueId);
    void PostDrawDispatch(
        bool isDraw);

    Device*                             m_pDevice;
    Platform*                           m_pPlatform;
    const size_t                        m_maxNumTracedDraws;
    struct
    {
        const Pipeline*                 pCurrentPipeline;
        uint64                          apiPsoHash;
    } m_pipeInfo[static_cast<uint32>(PipelineBindPoint::Count)];
    uint32                              m_currentDraw;
    uint32                              m_currentDispatch;
    uint32                              m_numTracedDraws;
    uint32                              m_numTracedDispatches;
    Util::Deque<TraceData, Platform>    m_traceData;

    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);
};

} // ShaderDbg
} // Pal
