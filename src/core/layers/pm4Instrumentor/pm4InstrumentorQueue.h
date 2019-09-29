/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "core/g_palPlatformSettings.h"
#include "core/layers/decorators.h"
#include "core/layers/functionIds.h"
#include "palVector.h"

namespace Pal
{
namespace Pm4Instrumentor
{

class CmdBuffer;
class Device;
class Platform;

// Number of distinct command buffer calls which are instrumented by this layer.
constexpr uint32 NumCallIds = static_cast<uint32>(CmdBufCallId::Count);

// Enumerates the "special" internal instrumentation events.
enum class InternalEventId : uint32
{
    UserDataValidationCs,   // Dispatch-time validation of user data entries
    UserDataValidationGfx,  // Draw-time validation of user-data entries

    PipelineValidationCs,   // Dispatch-time validation of pipeline state
    PipelineValidationGfx,  // Draw-time validation of pipeline state

    MiscDispatchValidation, // All Dispatch-time validation which doesn't fall into the above buckets
    MiscDrawValidation,     // All Draw-time validation which doesn't fall into the above buckets

    Count,
};

// Number of distinct internal instrumentation events.
constexpr uint32 NumEventIds = static_cast<uint32>(InternalEventId::Count);

// PM4 statistics for a single command buffer call or internal instrumentation event.
struct Pm4CallData
{
    gpusize  cmdSize;  // Total size of PM4 commands written by this entry point over the lifetime of the object.
    uint32   count;    // Number of times the command buffer entry point was called
};

// Contains PM4 statistics for a single command buffer, queue, or device.
struct Pm4Statistics
{
    Pm4CallData  call[NumCallIds];
    Pm4CallData  internalEvent[NumEventIds];

    gpusize  commandBufferSize; // Total amount of command buffer memory used over the lifetime of the object.
    gpusize  embeddedDataSize;  // Total amount of embedded data used over the lifetime of the object.
    gpusize  gpuScratchMemSize; // Total amount of GPU scratch memory used over the lifetime of the object.
};

// Contains a single record of a register for tracking usage within the PM4 optimizer.
struct RegisterInfo
{
    uint32  setPktTotal;    // Total number of SET and RMW packets seen for this register.
    uint32  setPktKept;     // Number of SET and RMW packets kept (after redundancy checking) for this register.
};

typedef Util::Vector<RegisterInfo, 1u, Platform>  RegisterInfoVector;

// =====================================================================================================================
// Pm4Instrumentor layer implementation of IQueue.  Accumulates stats from each submitted command buffer and dumps them
// to a log file.
class Queue : public QueueDecorator
{
public:
    Queue(IQueue*  pNextQueue,
          Device*  pDevice);

    Result Init();

    virtual Result Submit(
        const SubmitInfo& submitInfo) override;

private:
    virtual ~Queue();

    void AccumulateStatistics(
        const ICmdBuffer*const* ppCmdBuffers,
        uint32                  count);
    void DumpStatistics();

    Device*const  m_pDevice;

    Pm4Statistics  m_stats;
    uint32         m_cmdBufCount;

    RegisterInfoVector  m_shRegs;
    RegisterInfoVector  m_ctxRegs;

    uint16  m_shRegBase;
    uint16  m_ctxRegBase;

    Pm4InstrumentorDumpMode  m_dumpMode;
    int64                    m_dumpInterval;
    int64                    m_lastCpuPerfCounter;

    char  m_fileName[MaxPathStrLen << 1];

    PAL_DISALLOW_DEFAULT_CTOR(Queue);
    PAL_DISALLOW_COPY_AND_ASSIGN(Queue);
};

} // Pm4Instrumentor
} // Pal
