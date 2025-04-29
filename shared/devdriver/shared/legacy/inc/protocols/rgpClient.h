/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#include "legacyProtocolClient.h"

#include "protocols/rgpProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace RGPProtocol
    {
        typedef void(*TraceDataChunkReceived)(const TraceDataChunk* pChunk, void* pUserdata);

        struct ChunkCallbackInfo
        {
            TraceDataChunkReceived chunkCallback;
            void*                  pUserdata;
        };

        struct ClientTraceParametersInfo
        {
            uint32 gpuMemoryLimitInMb;
            uint32 numPreparationFrames;
            uint32 captureStartIndex;
            uint32 captureStopIndex;
            CaptureTriggerMode captureMode;

            union
            {
                struct
                {
                    uint32 enableInstructionTokens   : 1;
                    uint32 allowComputePresents      : 1;
                    uint32 captureDriverCodeObjects  : 1;
                    uint32 enableSpm                 : 1;
                    uint32 reserved : 28;
                };
                uint32 u32All;
            } flags;

            uint64 beginTag;
            uint64 endTag;

            char beginMarker[kMarkerStringLength];
            char endMarker[kMarkerStringLength];

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            uint64 pipelineHash;
#endif

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
            uint32 seMask;
#endif
        };

        struct BeginTraceInfo
        {
#if !DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            ClientTraceParametersInfo parameters;   // Parameters for the trace
#endif
            ChunkCallbackInfo         callbackInfo; // Callback used to return trace data
        };

        struct ClientSpmCounterId
        {
            uint32 blockId;
            uint32 instanceId;
            uint32 eventId;
        };

        struct ClientSpmConfig
        {
            uint32                    sampleFrequency;
            uint32                    memoryLimitInMb;
            uint32                    numCounters;
            const ClientSpmCounterId* pCounters;
        };

        class RGPClient : public LegacyProtocolClient
        {
        public:
            explicit RGPClient(IMsgChannel* pMsgChannel);
            ~RGPClient();

            // Requests an RGP trace in the driver. Returns Success if the request was successfully delivered.
            Result BeginTrace(const BeginTraceInfo& traceInfo);

            // Waits until a previously requested trace completes in the driver. Returns Result::NotReady if the
            // the timeout specified in timeoutInMs is exceeded. Returns the result of the trace otherwise. If the
            // trace was successful, the number of chunks generated is returned in pNumChunks and the size in
            // bytes of the trace data is returned in pTraceSizeInBytes.
            Result EndTrace(uint32* pNumChunks, uint64* pTraceSizeInBytes, uint32 timeoutInMs);

            // Reads a chunk of trace data from a previous trace that completed successfully. Returns chunk data
            // via the callback provided earlier in BeginTraceInfo.
            Result ReadTraceDataChunk();

            // Aborts a trace in progress.
            Result AbortTrace();

            // Queries the current profiling status of the driver.
            Result QueryProfilingStatus(ProfilingStatus* pStatus);

            // Enables profiling support inside the driver. ExecuteTrace will only succeed if the connected driver
            // has profiling enabled.
            Result EnableProfiling();

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            // Queries the connected driver's trace parameters.
            Result QueryTraceParameters(ClientTraceParametersInfo* pParameters);

            // Updates the connected driver's trace parameters.
            Result UpdateTraceParameters(const ClientTraceParametersInfo& parameters);
#endif

            Result UpdateCounterConfig(const ClientSpmConfig& config);

        private:
            void ResetState() override;
            bool ValidateInputCounter(const ClientSpmCounterId& counter) const;
            Result SendUpdateTraceParametersPacket(const ClientTraceParametersInfo& parameters);

            enum class TraceState : uint32
            {
                Idle = 0,
                TraceRequested,
                TraceCompleted,
                Error
            };

            struct ClientTraceContext
            {
                TraceState                state;
                BeginTraceInfo            traceInfo;
#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
                ClientTraceParametersInfo traceParameters;
#endif
                uint32                    numChunksReceived;
                uint32                    numChunks;
            };

            ClientTraceContext m_traceContext;

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            // Used by UpdateTraceParameters in back-compat mode to save the trace parameters
            // until a call to BeginTrace.
            ClientTraceParametersInfo m_tempTraceParameters;
#endif

            DD_STATIC_CONST uint32 kRGPChunkTimeoutInMs = 3000;
        };
    }
} // DevDriver
