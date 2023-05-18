/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "palVector.h"
#include "core/layers/decorators.h"

#include "crashAnalysis.h"

namespace Pal
{
namespace CrashAnalysis
{

class Device;
class Platform;

typedef Util::Vector<uint32, 8, Pal::IPlatform> MarkerStack;

// =====================================================================================================================
class CmdBuffer final : public CmdBufferFwdDecorator
{
public:
    CmdBuffer(ICmdBuffer*                pNextCmdBuffer,
              Device*                    pDevice,
              const CmdBufferCreateInfo& createInfo);

    MemoryChunk* GetMemoryChunk();

    // Public interface for marker insertion. Prefer to use
    // InsertBeginMarker and InsertEndMarker when possible.
    uint32 CmdInsertExecutionMarker(
        bool         isBegin,
        uint8        sourceId,
        const char*  pMarkerName,
        uint32       markerNameSize) override;

    // Generates a marker and writes the timestamp memory top-of-pipe
    // with the generated marker. Emits a Begin event through the Event Provider.
    uint32 InsertBeginMarker(
        MarkerSource source,
        const char*  pMarkerName,
        uint32       markerNameSize);

    // Pops off the most recent marker and writes it bottom-of-pipe to the timestamp
    // memory. Emits an End event through the Event Provider.
    uint32 InsertEndMarker(
        MarkerSource source);

    // Public ICmdBuffer interface methods:
    virtual Result Begin(
        const CmdBufferBuildInfo& info) override;

    virtual Result End() override;

    virtual Result Reset(
        ICmdAllocator* pCmdAllocator,
        bool           returnGpuMemory) override;

    virtual void Destroy() override;

    virtual void CmdExecuteNestedCmdBuffers(
        uint32            cmdBufferCount,
        ICmdBuffer*const* ppCmdBuffers) override;

    EventCache* GetEventCache();

private:
    virtual ~CmdBuffer();

    gpusize GetGpuVa(gpusize offset) const
    {
        PAL_ASSERT(m_pMemoryChunk != nullptr);
        return m_pMemoryChunk->gpuVirtAddr + offset;
    }

    void ResetState();
    void AddPreamble();
    void AddPostamble();

    // Pushes a marker onto a source-owned stack
    Result PushMarker(
        MarkerSource source,  // The marker source whichs 'owns' the stack to push onto.
        uint32       marker); // The marker value to push onto a stack.

    // Pops and returns a marker from the source-owned stack
    Result PopMarker(
        MarkerSource source,   // The marker source which 'owns' the marker to pop.
        uint32*      pMarker); // [out] The popped marker value

    // Issues a write call to update the current marker value
    void WriteMarkerImmediate(
        bool   isBegin,
        uint32 marker);

    // ICmdBuffer function table overrides:
    static void PAL_STDCALL CmdDrawDecorator(
        ICmdBuffer*       pCmdBuffer,
        uint32            firstVertex,
        uint32            vertexCount,
        uint32            firstInstance,
        uint32            instanceCount,
        uint32            drawId);
    static void PAL_STDCALL CmdDrawOpaqueDecorator(
        ICmdBuffer*       pCmdBuffer,
        gpusize           streamOutFilledSizeVa,
        uint32            streamOutOffset,
        uint32            stride,
        uint32            firstInstance,
        uint32            instanceCount);
    static void PAL_STDCALL CmdDrawIndexedDecorator(
        ICmdBuffer*       pCmdBuffer,
        uint32            firstIndex,
        uint32            indexCount,
        int32             vertexOffset,
        uint32            firstInstance,
        uint32            instanceCount,
        uint32            drawId);
    static void PAL_STDCALL CmdDrawIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDrawIndexedIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);
    static void PAL_STDCALL CmdDispatchDecorator(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size);
    static void PAL_STDCALL CmdDispatchIndirectDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset);
    static void PAL_STDCALL CmdDispatchOffsetDecorator(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      offset,
        DispatchDims      launchSize,
        DispatchDims      logicalSize);
    static void PAL_STDCALL CmdDispatchDynamicDecorator(
        ICmdBuffer*       pCmdBuffer,
        gpusize           gpuVa,
        DispatchDims      size);
    static void PAL_STDCALL CmdDispatchMeshDecorator(
        ICmdBuffer*       pCmdBuffer,
        DispatchDims      size);
    static void PAL_STDCALL CmdDispatchMeshIndirectMultiDecorator(
        ICmdBuffer*       pCmdBuffer,
        const IGpuMemory& gpuMemory,
        gpusize           offset,
        uint32            stride,
        uint32            maximumCount,
        gpusize           countGpuAddr);

    Device*const   m_pDevice;
    Platform*const m_pPlatform;
    uint32         m_cmdBufferId;
    uint32         m_markerCounter;
    MemoryChunk*   m_pMemoryChunk;
    EventCache*    m_pEventCache;
    Util::Vector<MarkerStack, MarkerStackCount, IPlatform> m_markerStack;

    PAL_DISALLOW_DEFAULT_CTOR(CmdBuffer);
    PAL_DISALLOW_COPY_AND_ASSIGN(CmdBuffer);

};

} // namespace CrashAnalysis
} // namespace Pal
