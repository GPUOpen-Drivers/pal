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

#include "protocols/rgpClient.h"
#include "msgChannel.h"

#include <string.h>

#define RGP_CLIENT_MIN_VERSION 2
#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
    #define RGP_CLIENT_MAX_VERSION 11
#else
    #define RGP_CLIENT_MAX_VERSION 9
#endif

namespace DevDriver
{
    namespace RGPProtocol
    {
        void EncodeTraceParameters(TraceParametersV6* pParameters, const ClientTraceParametersInfo& parameters)
        {
            DD_ASSERT(pParameters != nullptr);

            pParameters->gpuMemoryLimitInMb   = parameters.gpuMemoryLimitInMb;
            pParameters->numPreparationFrames = parameters.numPreparationFrames;

            pParameters->captureStartIndex    = parameters.captureStartIndex;
            pParameters->captureStopIndex     = parameters.captureStopIndex;

            pParameters->captureMode          = parameters.captureMode;

            pParameters->flags.u32All         = parameters.flags.u32All;

            pParameters->beginTagHigh = static_cast<uint32>(parameters.beginTag >> 32);
            pParameters->beginTagLow  = static_cast<uint32>(parameters.beginTag & 0xFFFFFFFF);
            pParameters->endTagHigh = static_cast<uint32>(parameters.endTag >> 32);
            pParameters->endTagLow  = static_cast<uint32>(parameters.endTag & 0xFFFFFFFF);

            Platform::Strncpy(pParameters->beginMarker,
                              parameters.beginMarker,
                              sizeof(pParameters->beginMarker));

            Platform::Strncpy(pParameters->endMarker,
                              parameters.endMarker,
                              sizeof(pParameters->endMarker));

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            pParameters->pipelineHashHi =
                static_cast<uint32>(parameters.pipelineHash >> 32);
            pParameters->pipelineHashLo =
                static_cast<uint32>(parameters.pipelineHash & 0xFFFFFFFF);
#else
            pParameters->pipelineHashHi = 0;
            pParameters->pipelineHashLo = 0;
#endif
        }

        void EncodeTraceParameters(TraceParametersV7* pParameters, const ClientTraceParametersInfo& parameters)
        {
            DD_ASSERT(pParameters != nullptr);

            pParameters->gpuMemoryLimitInMb   = parameters.gpuMemoryLimitInMb;
            pParameters->numPreparationFrames = parameters.numPreparationFrames;

            pParameters->captureStartIndex    = parameters.captureStartIndex;
            pParameters->captureStopIndex     = parameters.captureStopIndex;

            pParameters->captureMode          = parameters.captureMode;

            pParameters->flags.u32All         = parameters.flags.u32All;

            pParameters->beginTagHigh         = static_cast<uint32>(parameters.beginTag >> 32);
            pParameters->beginTagLow          = static_cast<uint32>(parameters.beginTag & 0xFFFFFFFF);
            pParameters->endTagHigh           = static_cast<uint32>(parameters.endTag >> 32);
            pParameters->endTagLow            = static_cast<uint32>(parameters.endTag & 0xFFFFFFFF);

            Platform::Strncpy(pParameters->beginMarker,
                              parameters.beginMarker,
                              sizeof(pParameters->beginMarker));

            Platform::Strncpy(pParameters->endMarker,
                              parameters.endMarker,
                              sizeof(pParameters->endMarker));

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            pParameters->pipelineHashHi =
                static_cast<uint32>(parameters.pipelineHash >> 32);
            pParameters->pipelineHashLo =
                static_cast<uint32>(parameters.pipelineHash & 0xFFFFFFFF);
#else
            pParameters->pipelineHashHi = 0;
            pParameters->pipelineHashLo = 0;
#endif

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
            pParameters->seMask = parameters.seMask;
#else
            pParameters->seMask = 0xFFFFFFFF;
#endif
        }

        RGPClient::RGPClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::RGP, RGP_CLIENT_MIN_VERSION, RGP_CLIENT_MAX_VERSION)
        {
            memset(&m_traceContext, 0, sizeof(m_traceContext));
#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            memset(&m_tempTraceParameters, 0, sizeof(m_tempTraceParameters));
#endif
        }

        RGPClient::~RGPClient()
        {
        }

        Result RGPClient::BeginTrace(const BeginTraceInfo& traceInfo)
        {
            Result result = Result::Error;

            if ((m_traceContext.state == TraceState::Idle) &&
                (traceInfo.callbackInfo.chunkCallback != nullptr))
            {
                result = Result::Success;

                RGPPayload payload = {};
                payload.command = RGPMessage::ExecuteTraceRequest;

                if (GetSessionVersion() < RGP_DECOUPLED_TRACE_PARAMETERS)
                {
                    // We're connected to a legacy server, fill out the trace parameters in the packet.

#if !DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
                    // The caller is using the old API so source the parameters from the function input.
                    const ClientTraceParametersInfo& parameters = traceInfo.parameters;
#else
                    // The caller is using the new API so source the parameters from our saved parameters.
                    const ClientTraceParametersInfo& parameters = m_tempTraceParameters;
#endif

                    if (GetSessionVersion() < RGP_PROFILING_CLOCK_MODES_VERSION)
                    {
                        payload.executeTraceRequest.parameters.gpuMemoryLimitInMb = parameters.gpuMemoryLimitInMb;
                        payload.executeTraceRequest.parameters.numPreparationFrames = parameters.numPreparationFrames;
                        payload.executeTraceRequest.parameters.flags.u32All = parameters.flags.u32All;
                    }
                    else if ((GetSessionVersion() == RGP_PROFILING_CLOCK_MODES_VERSION) ||
                             (GetSessionVersion() == RGP_TRACE_PROGRESS_VERSION))
                    {
                        payload.executeTraceRequestV2.parameters.gpuMemoryLimitInMb = parameters.gpuMemoryLimitInMb;
                        payload.executeTraceRequestV2.parameters.numPreparationFrames = parameters.numPreparationFrames;
                        payload.executeTraceRequestV2.parameters.clockMode = ProfilingClockMode::Stable;
                        payload.executeTraceRequestV2.parameters.flags.u32All = parameters.flags.u32All;
                    }
                    else if (GetSessionVersion() == RGP_COMPUTE_PRESENTS_VERSION)
                    {
                        payload.executeTraceRequestV3.parameters.gpuMemoryLimitInMb = parameters.gpuMemoryLimitInMb;
                        payload.executeTraceRequestV3.parameters.numPreparationFrames = parameters.numPreparationFrames;
                        payload.executeTraceRequestV3.parameters.flags.u32All = parameters.flags.u32All;
                    }
                    else if ((GetSessionVersion() == RGP_TRIGGER_MARKERS_VERSION) ||
                             (GetSessionVersion() == RGP_PENDING_ABORT_VERSION))
                    {
                        payload.executeTraceRequestV4.parameters.gpuMemoryLimitInMb = parameters.gpuMemoryLimitInMb;
                        payload.executeTraceRequestV4.parameters.numPreparationFrames = parameters.numPreparationFrames;
                        payload.executeTraceRequestV4.parameters.flags.u32All = parameters.flags.u32All;

                        payload.executeTraceRequestV4.parameters.beginTagLow =
                            static_cast<uint32>(parameters.beginTag & 0xFFFFFFFF);
                        payload.executeTraceRequestV4.parameters.beginTagHigh =
                            static_cast<uint32>((parameters.beginTag >> 32) & 0xFFFFFFFF);

                        payload.executeTraceRequestV4.parameters.endTagLow =
                            static_cast<uint32>(parameters.endTag & 0xFFFFFFFF);
                        payload.executeTraceRequestV4.parameters.endTagHigh =
                            static_cast<uint32>((parameters.endTag >> 32) & 0xFFFFFFFF);

                        Platform::Strncpy(payload.executeTraceRequestV4.parameters.beginMarker,
                                          parameters.beginMarker,
                                          sizeof(payload.executeTraceRequestV4.parameters.beginMarker));

                        Platform::Strncpy(payload.executeTraceRequestV4.parameters.endMarker,
                                          parameters.endMarker,
                                          sizeof(payload.executeTraceRequestV4.parameters.endMarker));
                    }
                    else if (GetSessionVersion() == RGP_FRAME_CAPTURE_VERSION)
                    {
                        payload.executeTraceRequestV5.parameters.gpuMemoryLimitInMb = parameters.gpuMemoryLimitInMb;
                        payload.executeTraceRequestV5.parameters.numPreparationFrames = parameters.numPreparationFrames;
                        payload.executeTraceRequestV5.parameters.captureMode = parameters.captureMode;
                        payload.executeTraceRequestV5.parameters.flags.u32All = parameters.flags.u32All;

                        payload.executeTraceRequestV5.parameters.captureStartIndex = parameters.captureStartIndex;
                        payload.executeTraceRequestV5.parameters.captureStopIndex  = parameters.captureStopIndex;

                        payload.executeTraceRequestV5.parameters.beginTagLow =
                            static_cast<uint32>(parameters.beginTag & 0xFFFFFFFF);
                        payload.executeTraceRequestV5.parameters.beginTagHigh =
                            static_cast<uint32>((parameters.beginTag >> 32) & 0xFFFFFFFF);

                        payload.executeTraceRequestV5.parameters.endTagLow =
                            static_cast<uint32>(parameters.endTag & 0xFFFFFFFF);
                        payload.executeTraceRequestV5.parameters.endTagHigh =
                            static_cast<uint32>((parameters.endTag >> 32) & 0xFFFFFFFF);

                        Platform::Strncpy(payload.executeTraceRequestV5.parameters.beginMarker,
                                          parameters.beginMarker,
                                          sizeof(payload.executeTraceRequestV5.parameters.beginMarker));

                        Platform::Strncpy(payload.executeTraceRequestV5.parameters.endMarker,
                                          parameters.endMarker,
                                          sizeof(payload.executeTraceRequestV5.parameters.endMarker));
                    }
                }
                else if (GetSessionVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS)
                {
                    // The server is using a new version of the protocol that decouples the trace parameters
                    // from execute trace.

#if !DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
                    // The caller is using the old API so we need to update the server's trace parameters
                    // using the parameters provided in the function input before we execute the trace.
                    result = SendUpdateTraceParametersPacket(parameters);
#else
                    // The caller is using the new API so we don't need to do anything here since the server
                    // already has the latest parameters.
#endif
                }
                else
                {
                    // Unhandled protocol version
                    DD_UNREACHABLE();
                }

                // If the parameters were handled successfully, send the execute trace request.
                if (result == Result::Success)
                {
                    result = SendPayload(&payload);
                }

                if (result == Result::Success)
                {
                    m_traceContext.traceInfo = traceInfo;

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
                    // Save the current copy of the trace parameters into our trace context.
                    m_traceContext.traceParameters = m_tempTraceParameters;
#endif
                    m_traceContext.state = TraceState::TraceRequested;
                }
                else
                {
                    // If we fail to send the payload, fail the trace.
                    m_traceContext.state = TraceState::Error;
                    result = Result::Error;
                }
            }

            return result;
        }

        Result RGPClient::EndTrace(uint32* pNumChunks, uint64* pTraceSizeInBytes, uint32 timeoutInMs)
        {
            Result result = Result::Error;

            if ((m_traceContext.state == TraceState::TraceRequested) &&
                (pNumChunks != nullptr)                              &&
                (pTraceSizeInBytes != nullptr))
            {
                if (GetSessionVersion() >= RGP_TRACE_PROGRESS_VERSION)
                {
                    RGPPayload payload = {};

                    // Attempt to receive the trace data header.
                    result = ReceivePayload(&payload, timeoutInMs);
                    if ((result == Result::Success) && (payload.command == RGPMessage::TraceDataHeader))
                    {
                        // We've successfully received the trace data header. Check if the trace was successful.
                        result = payload.traceDataHeader.result;
                        if (result == Result::Success)
                        {
                            m_traceContext.state = TraceState::TraceCompleted;
                            m_traceContext.numChunks = payload.traceDataHeader.numChunks;
                            m_traceContext.numChunksReceived = 0;

                            *pNumChunks = payload.traceDataHeader.numChunks;
                            *pTraceSizeInBytes = payload.traceDataHeader.sizeInBytes;
                        }
                        else
                        {
                            // Reset the trace state.
                            m_traceContext.state = TraceState::Error;

                            // Don't overwrite the result from the trace header here. We want to return that to the caller.
                        }
                    }
                    else if (result == Result::NotReady)
                    {
                        // If we hit the user specified timeout, don't modify the trace state.
                        // Just return the result to the caller.
                    }
                    else
                    {
                        m_traceContext.state = TraceState::Error;
                        result = Result::Error;
                    }
                }
                else
                {
                    m_traceContext.state = TraceState::TraceCompleted;
                    result = Result::Unavailable;
                }
            }

            return result;
        }

        Result RGPClient::ReadTraceDataChunk()
        {
            Result result = Result::Error;

            RGPPayload payload = {};

            if (m_traceContext.state == TraceState::TraceCompleted)
            {
                if (GetSessionVersion() >= RGP_TRACE_PROGRESS_VERSION)
                {
                    result = ReceivePayload(&payload, kRGPChunkTimeoutInMs);

                    if (result == Result::Success)
                    {
                        if ((payload.command == RGPMessage::TraceDataChunk) && (m_traceContext.numChunksReceived < m_traceContext.numChunks))
                        {
                            // Call the chunk callback with the trace data.
                            m_traceContext.traceInfo.callbackInfo.chunkCallback(&payload.traceDataChunk.chunk, m_traceContext.traceInfo.callbackInfo.pUserdata);

                            ++m_traceContext.numChunksReceived;

                            // If we have no chunks left, the trace process was successfully completed.
                            if (m_traceContext.numChunksReceived == m_traceContext.numChunks)
                            {
                                // Make sure we read the sentinel value before returning. It should always mark the end of the trace data chunk stream.
                                result = ReceivePayload(&payload, kRGPChunkTimeoutInMs);

                                if ((result == Result::Success) && (payload.command == RGPMessage::TraceDataSentinel))
                                {
                                    result = Result::EndOfStream;
                                    m_traceContext.state = TraceState::Idle;
                                }
                                else
                                {
                                    // Failed to receive a trace data chunk. Fail the trace.
                                    m_traceContext.state = TraceState::Error;
                                    result = Result::Error;
                                }
                            }
                        }
                        else
                        {
                            // Failed to receive a trace data chunk. Fail the trace.
                            m_traceContext.state = TraceState::Error;
                            result = Result::Error;
                        }
                    }
                    else
                    {
                        // Failed to receive a trace data chunk. Fail the trace.
                        m_traceContext.state = TraceState::Error;
                        result = Result::Error;
                    }
                }
                else if (GetSessionVersion() < RGP_TRACE_PROGRESS_VERSION)
                {
#if !DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
                    const uint32 numPrepFrames = m_traceContext.traceInfo.parameters.numPreparationFrames;
#else
                    const uint32 numPrepFrames = m_traceContext.traceParameters.numPreparationFrames;
#endif
                    const uint32 firstChunkTimeout = kRGPChunkTimeoutInMs * (numPrepFrames + 1);
                    const uint32 packetTimeout = (m_traceContext.numChunksReceived == 0) ? firstChunkTimeout : kRGPChunkTimeoutInMs;

                    result = ReceivePayload(&payload, packetTimeout);

                    if (result == Result::Success)
                    {
                        if (payload.command == RGPMessage::TraceDataChunk)
                        {
                            // Call the chunk callback with the trace data.
                            m_traceContext.traceInfo.callbackInfo.chunkCallback(&payload.traceDataChunk.chunk, m_traceContext.traceInfo.callbackInfo.pUserdata);

                            ++m_traceContext.numChunksReceived;
                        }
                        else if (payload.command == RGPMessage::TraceDataSentinel)
                        {
                            result = Result::EndOfStream;
                        }
                    }
                    else
                    {
                        // Failed to receive a trace data chunk. Fail the trace.
                        m_traceContext.state = TraceState::Error;
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result RGPClient::AbortTrace()
        {
            Result result = Result::Error;

            RGPPayload payload = {};

            if ((m_traceContext.state == TraceState::TraceCompleted) |
                ((m_traceContext.state == TraceState::TraceRequested) & (GetSessionVersion() >= RGP_PENDING_ABORT_VERSION)))
            {
                if (GetSessionVersion() >= RGP_TRACE_PROGRESS_VERSION)
                {
                    payload.command = RGPMessage::AbortTrace;

                    result = SendPayload(&payload);

                    if (result == Result::Success)
                    {
                        // Discard all messages until we find the trace data sentinel.
                        while ((result == Result::Success) && (payload.command != RGPMessage::TraceDataSentinel))
                        {
                            result = ReceivePayload(&payload);
                        }

                        if ((result == Result::Success)                        &&
                            (payload.command == RGPMessage::TraceDataSentinel) &&
                            (payload.traceDataSentinel.result == Result::Aborted))
                        {
                            // We've successfully aborted the trace.
                            m_traceContext.state = TraceState::Idle;
                        }
                        else
                        {
                            // Fail the trace if this process does not succeed.
                            m_traceContext.state = TraceState::Error;
                            result = Result::Error;
                        }
                    }
                    else
                    {
                        // If we fail to send the payload, fail the trace.
                        m_traceContext.state = TraceState::Error;
                        result = Result::Error;
                    }
                }
                else
                {
                    // Support for aborting traces is not available until the trace progress version.
                    result = Result::Unavailable;
                }
            }

            return result;
        }

        Result RGPClient::QueryProfilingStatus(ProfilingStatus* pStatus)
        {
            Result result = Result::Error;

            if ((pStatus != nullptr) && IsConnected())
            {
                RGPPayload payload = {};
                payload.command = RGPMessage::QueryProfilingStatusRequest;

                if ((Transact(&payload) == Result::Success) &&
                    (payload.command == RGPMessage::QueryProfilingStatusResponse))
                {
                    *pStatus = payload.queryProfilingStatusResponse.status;
                    result = Result::Success;
                }
            }

            return result;
        }

        Result RGPClient::EnableProfiling()
        {
            Result result = Result::Error;

            if (IsConnected())
            {
                RGPPayload payload = {};
                payload.command = RGPMessage::EnableProfilingRequest;

                if ((Transact(&payload) == Result::Success) &&
                    (payload.command == RGPMessage::EnableProfilingResponse))
                {
                    result = payload.enableProfilingStatusResponse.result;
                }
            }

            return result;
        }

#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
        Result RGPClient::QueryTraceParameters(ClientTraceParametersInfo* pParameters)
        {
            DD_ASSERT(pParameters != nullptr);

            // Default to error in the case that we're not currently connected.
            Result result = Result::Error;

            if (IsConnected())
            {
                if (GetSessionVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS)
                {
                    RGPPayload payload = {};
                    payload.command = RGPMessage::QueryTraceParametersRequest;

                    if ((Transact(&payload) == Result::Success) &&
                        (payload.command == RGPMessage::QueryTraceParametersResponse))
                    {
                        result = payload.queryTraceParametersResponse.result;
                        if (result == Result::Success)
                        {
                            if (GetSessionVersion() == RGP_DECOUPLED_TRACE_PARAMETERS)
                            {
                                const TraceParametersV6& traceParameters =
                                    payload.queryTraceParametersResponse.parameters;

                                pParameters->gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                pParameters->numPreparationFrames = traceParameters.numPreparationFrames;
                                pParameters->captureMode = traceParameters.captureMode;
                                pParameters->flags.u32All = traceParameters.flags.u32All;

                                pParameters->captureStartIndex = traceParameters.captureStartIndex;
                                pParameters->captureStopIndex = traceParameters.captureStopIndex;

                                pParameters->beginTag =
                                    ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) |
                                        traceParameters.beginTagLow);
                                pParameters->endTag =
                                    ((static_cast<uint64>(traceParameters.endTagHigh) << 32) |
                                        traceParameters.endTagLow);

                                Platform::Strncpy(pParameters->beginMarker,
                                    traceParameters.beginMarker,
                                    sizeof(pParameters->beginMarker));

                                Platform::Strncpy(pParameters->endMarker,
                                    traceParameters.endMarker,
                                    sizeof(pParameters->endMarker));

                                pParameters->pipelineHash =
                                    ((static_cast<uint64>(traceParameters.pipelineHashHi) << 32) |
                                        static_cast<uint64>(traceParameters.pipelineHashLo));
                            }
                            else
                            {
                                const TraceParametersV7& traceParameters =
                                    payload.queryTraceParametersResponseV2.parameters;

                                pParameters->gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                pParameters->numPreparationFrames = traceParameters.numPreparationFrames;
                                pParameters->captureMode = traceParameters.captureMode;
                                pParameters->flags.u32All = traceParameters.flags.u32All;

                                pParameters->captureStartIndex = traceParameters.captureStartIndex;
                                pParameters->captureStopIndex = traceParameters.captureStopIndex;

                                pParameters->beginTag =
                                    ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) |
                                        traceParameters.beginTagLow);
                                pParameters->endTag =
                                    ((static_cast<uint64>(traceParameters.endTagHigh) << 32) |
                                        traceParameters.endTagLow);

                                Platform::Strncpy(pParameters->beginMarker,
                                    traceParameters.beginMarker,
                                    sizeof(pParameters->beginMarker));

                                Platform::Strncpy(pParameters->endMarker,
                                    traceParameters.endMarker,
                                    sizeof(pParameters->endMarker));

                                pParameters->pipelineHash =
                                    ((static_cast<uint64>(traceParameters.pipelineHashHi) << 32) |
                                        static_cast<uint64>(traceParameters.pipelineHashLo));

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
                                pParameters->seMask = traceParameters.seMask;
#endif
                            }
                        }
                    }
                }
                else
                {
                    // We're connected to an older server so we can't properly implement this function.
                    // Just return the most recently cached copy as an "approximation".
                    *pParameters = m_tempTraceParameters;
                    result = Result::Success;
                }
            }

            return result;
        }

        Result RGPClient::UpdateTraceParameters(const ClientTraceParametersInfo& parameters)
        {
            // Default to error in the case that we're not currently connected.
            Result result = Result::Error;

            if (IsConnected())
            {
                ClientTraceParametersInfo mutableParams = parameters;
                // Clients with older RGP version don't support the detailed SEMask, so we clear it from the trace parameters
                if (GetSessionVersion() < RGP_DETAILED_SEMASK_VERSION)
                {
                    mutableParams.seMask = 0;
                }

                if (GetSessionVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS)
                {
                    result = SendUpdateTraceParametersPacket(mutableParams);
                }
                else
                {
                    // We're connected to an older server so we have nothing to do here.
                    result = Result::Success;
                }

                // If everything is successful, cache the most recent version of the trace parameters.
                // We have to keep this copy around to handle back-compat.
                if (result == Result::Success)
                {
                    m_tempTraceParameters = mutableParams;
                }
            }

            return result;
        }
#endif

        Result RGPClient::UpdateCounterConfig(const ClientSpmConfig& config)
        {
            // Default to error in the case that we're not currently connected.
            Result result = Result::Error;

            if (IsConnected())
            {
                if (GetSessionVersion() >= RGP_SPM_COUNTERS_VERSION)
                {
                    result = Result::Success;

                    // Validate all the counters
                    for (uint32 counterIndex = 0; counterIndex < config.numCounters; ++counterIndex)
                    {
                        const ClientSpmCounterId& inputCounter = config.pCounters[counterIndex];

                        // Make sure the input counter will fit into the network packet
                        if (ValidateInputCounter(inputCounter) == false)
                        {
                            result = Result::InvalidParameter;
                            break;
                        }
                    }

                    RGPPayload payload = {};
                    const uint32 numDataPayloads = (config.numCounters / kMaxSpmCountersPerUpdate) +
                        (((config.numCounters % kMaxSpmCountersPerUpdate) == 0) ? 0 : 1);

                    if (result == Result::Success)
                    {
                        payload.command = RGPMessage::UpdateSpmConfigRequest;
                        payload.updateSpmConfigRequest.sampleFrequency = config.sampleFrequency;
                        payload.updateSpmConfigRequest.memoryLimitInMb = config.memoryLimitInMb;
                        payload.updateSpmConfigRequest.numDataPayloads = numDataPayloads;

                        result = SendPayload(&payload);
                    }

                    if (result == Result::Success)
                    {
                        payload.command = RGPMessage::UpdateSpmConfigData;

                        for (uint32 dataPayloadIndex = 0; dataPayloadIndex < numDataPayloads; ++dataPayloadIndex)
                        {
                            const uint32 baseCounterOffset = (dataPayloadIndex * kMaxSpmCountersPerUpdate);
                            const uint32 numCountersInPacket = Platform::Min(kMaxSpmCountersPerUpdate, (config.numCounters - baseCounterOffset));

                            payload.updateSpmConfigData.numCounters = numCountersInPacket;

                            for (uint32 counterIndex = 0; counterIndex < numCountersInPacket; ++counterIndex)
                            {
                                const ClientSpmCounterId& inputCounter = config.pCounters[baseCounterOffset + counterIndex];
                                SpmCounterId& payloadCounter = payload.updateSpmConfigData.counters[counterIndex];

                                payloadCounter.blockId = inputCounter.blockId;
                                payloadCounter.instanceId = inputCounter.instanceId;
                                payloadCounter.eventId = inputCounter.eventId;
                            }

                            result = SendPayload(&payload);

                            if (result != Result::Success)
                            {
                                break;
                            }
                        }
                    }

                    if (result == Result::Success)
                    {
                        result = ReceivePayload(&payload);
                    }

                    if (result == Result::Success)
                    {
                        if (payload.command == RGPMessage::UpdateSpmConfigResponse)
                        {
                            result = payload.updateSpmConfigResponse.result;
                        }
                        else
                        {
                            // Invalid response type
                            result = Result::Error;
                        }
                    }
                }
                else
                {
                    // We're connected to an older server so we can't use this functionality
                    result = Result::VersionMismatch;
                }
            }

            return result;
        }

        void RGPClient::ResetState()
        {
            memset(&m_traceContext, 0, sizeof(m_traceContext));
#if DD_VERSION_SUPPORTS(GPUOPEN_DECOUPLED_RGP_PARAMETERS_VERSION)
            memset(&m_tempTraceParameters, 0, sizeof(m_tempTraceParameters));
#endif
        }

        bool RGPClient::ValidateInputCounter(const ClientSpmCounterId& counter) const
        {
            // Return true if the input counter fields will fit into the network packet
            return ((counter.blockId    < kMaxSpmBlockId)    &&
                    (counter.instanceId < kMaxSpmInstanceId) &&
                    (counter.eventId    < kMaxSpmEventId));
        }

        Result RGPClient::SendUpdateTraceParametersPacket(const ClientTraceParametersInfo& parameters)
        {
            Result result = Result::Error;

            RGPPayload payload = {};
            payload.command = RGPMessage::UpdateTraceParametersRequest;

            if (GetSessionVersion() == RGP_DECOUPLED_TRACE_PARAMETERS)
            {
                TraceParametersV6& payloadParams =
                    payload.updateTraceParametersRequest.parameters;

                EncodeTraceParameters(&payloadParams, parameters);
            }
            else
            {
                TraceParametersV7& payloadParams =
                    payload.updateTraceParametersRequestV2.parameters;

                EncodeTraceParameters(&payloadParams, parameters);
            }

            if ((Transact(&payload) == Result::Success) &&
                (payload.command == RGPMessage::UpdateTraceParametersResponse))
            {
                result = payload.updateTraceParametersResponse.result;
            }

            return result;
        }
    }

} // DevDriver
