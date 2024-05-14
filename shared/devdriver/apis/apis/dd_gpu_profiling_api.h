/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023-2024 Advanced Micro Devices, Inc. All Rights Reserved.
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

#ifndef DD_GPU_PROFILING_API_H
#define DD_GPU_PROFILING_API_H

#include "dd_common_api.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define DD_GPU_PROFILING_API_NAME      "DD_GPU_PROFILING_API"
#define DD_GPU_PROFILING_API_VERSION_MAJOR 1
#define DD_GPU_PROFILING_API_VERSION_MINOR 0
#define DD_GPU_PROFILING_API_VERSION_PATCH 0

typedef struct DDGpuProfilingInstance DDGpuProfilingInstance;

/// Struct containing data that describes a specific spm counter
typedef struct DDGpuProfilingSpmCounterId
{
    uint32_t blockId;    /// Identifies the target hardware block
    uint32_t instanceId; /// Identifies the desired instance of the hardware block
    uint32_t eventId;    /// Identifies the desired event
} DDGpuProfilingSpmCounterId;

typedef enum DDGpuProfilingTriggerMode : uint32_t
{
    DD_GPU_PROFILING_TRIGGER_MODE_UNKNOWN         = 0, /// Unknown trigger mode
    DD_GPU_PROFILING_TRIGGER_MODE_PRESENT         = 1, /// Triggered when the application presents a frame
    DD_GPU_PROFILING_TRIGGER_MODE_MARKER          = 2, /// Triggered when specific user markers are encountered
    DD_GPU_PROFILING_TRIGGER_MODE_TAG             = 3, /// Triggered when specific command buffer tags are encountered
    DD_GPU_PROFILING_TRIGGER_MODE_FRAME_INDEX     = 4, /// Triggered when a specific frame index is reached
    DD_GPU_PROFILING_TRIGGER_MODE_DISPATCH_INDEX  = 5, /// Started/stopped when specific dispatch indices are reached

    DD_GPU_PROFILING_TRIGGER_MODE_COUNT /// Total number of trigger modes
} DDGpuProfilingTriggerMode;

static constexpr uint32_t kDDGpuProfilingConfigMarkerStringLen = 256; /// Mirrors rgpClient.h

struct DDGpuProfilingConfig
{
    uint32_t                  gpuMemoryLimitInMb;
    uint32_t                  numPreparationFrames;
    DDGpuProfilingTriggerMode captureMode;

    union
    {
        struct
        {
            uint32_t enableInstructionTokens  : 1;
            uint32_t allowComputePresents     : 1;
            uint32_t captureDriverCodeObjects : 1;
            uint32_t enableSpm                : 1;
            uint32_t reserved                 : 28;
        };
        uint32_t u32All;
    } flags;

    uint32_t captureStartIndex;
    uint32_t captureStopIndex;

    uint64_t captureStartTag;
    uint64_t captureStopTag;

    char captureStartMarker[kDDGpuProfilingConfigMarkerStringLen];
    char captureStopMarker[kDDGpuProfilingConfigMarkerStringLen];

    uint64_t instructionTraceApiPsoHash;
    uint32_t shaderEngineInstructionTraceMask;

    uint32_t spmSampleFrequency;
    uint32_t spmMemoryLimit;
};

/// Struct containing arguments to execute a profiling trace
typedef struct DDGpuProfilingTraceArgs
{
    uint32_t     timerDuration; /// Timer duration for timer based capture modes
    uint32_t     timeoutInMs;   /// Timeout value in milliseconds for the trace
    DDByteWriter writer;        /// Callbacks to receive trace data

    void (*pPostBeginTraceCallback)(void* pUserdata); /// Callback to call after tracing has begun.
    void* pPostBeginTraceUserdata;                        /// Userdata to call pPostBeginTraceCallback with.

    DDGpuProfilingConfig config; /// Config for trace

} DDGpuProfilingTraceArgs;

#define DD_GPU_PROFILING_SPM_ALL_INSTANCES 0xFFFFFFFF

typedef struct DDGpuProfilingApi
{
    /// An opaque pointer to internal implementation of Profiling API.
    DDGpuProfilingInstance* pInstance;

    /// @brief Enables tracing with the specified client.
    /// @param pInstance Must be @ref DDGpuProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    /// @param pConfig The initial configuration for the client.
    /// @return DD_RESULT_SUCCESS on successful connection
    /// @return DD_RESULT_DD_GENERIC_NOT_READY if router connection isn't ready.
    /// @return Other error codes if connection failed.
    DD_RESULT (*EnableTracing)(DDGpuProfilingInstance* pInstance, DDConnectionId umdConnectionId, const DDGpuProfilingConfig* pConfig);

    /// @brief Disables tracing on the specified client
    /// @param pInstance Must be @ref DDGpuProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    void (*DisableTracing)(DDGpuProfilingInstance* pInstance, DDConnectionId umdConnectionId);

    /// @brief Executes a trace on a connected client.
    /// @param pInstance Must be @ref DDGpuProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    /// @param pArgs The trace arguments.
    /// @return DD_RESULT_SUCCESS if tracing competed successfully.
    /// @return Other error codes if tracing failed.
    DD_RESULT(*ExecuteTrace)(
        DDGpuProfilingInstance*        pInstance,
        DDConnectionId                 umdConnectionId,
        const DDGpuProfilingTraceArgs* pArgs);

    /// @brief Aborts a trace on a connected client.
    /// @param pInstance Must be @ref DDGpuProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    void(*AbortTrace)(DDGpuProfilingInstance* pInstance, DDConnectionId umdConnectionId);

    /// @brief Sets the list of SPM counters to query during capture.
    /// @param pInstance Must be @ref DDGpuProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    /// @param pCounters The list of SPM counters to query.
    /// @param numCounters The size of SPM counter list.
    /// @return DD_RESULT_SUCCESS or error code on failure.
    DD_RESULT(*SetSpmCounters)(
        DDGpuProfilingInstance*           pInstance,
        DDConnectionId                    umdConnectionId,
        const DDGpuProfilingSpmCounterId* pCounters,
        uint32_t                          numCounters);

    /// @brief Queries the RGP client protocol version from connected client.
    /// @param pInstance Must be @ref DDProfilingApi.pInstance.
    /// @param umdConnectionId The client connection identifier.
    /// @param pVersion The protocol version.
    /// @return DD_RESULT_SUCCESS if query completed.
    /// @return Other error codes if query failed.
    DD_RESULT(*QueryClientProtocolVersion)(
        DDGpuProfilingInstance* pInstance,
        DDConnectionId          umdConnectionId,
        uint16_t*               pVersion);
} DDGpuProfilingApi;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
