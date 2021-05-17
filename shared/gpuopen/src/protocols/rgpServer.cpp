/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#include "protocols/rgpServer.h"
#include "msgChannel.h"

#include "util/queue.h"

#define RGP_SERVER_MIN_VERSION 2
#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
    #define RGP_SERVER_MAX_VERSION 11
#else
    #define RGP_SERVER_MAX_VERSION 9
#endif

namespace DevDriver
{
    namespace RGPProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            TransferTraceData,
            TransferSpmConfigData,
        };

        struct RGPSession
        {
            SessionState               state;
            Version                    version;
            uint64                     traceSizeInBytes;
            Queue<RGPPayload, 32>      chunkPayloads;
            RGPPayload                 payload;
            bool                       abortRequestedByClient;
            uint32                     numSpmUpdatePackets;
            ServerSpmConfig            spmConfig;
            Vector<ServerSpmCounterId> spmCounters;

            explicit RGPSession(const AllocCb& allocCb)
                : state(SessionState::ReceivePayload)
                , version(0)
                , traceSizeInBytes(0)
                , chunkPayloads(allocCb)
                , payload()
                , abortRequestedByClient(false)
                , numSpmUpdatePackets(0)
                , spmConfig()
                , spmCounters(allocCb)
            {
            }
        };

        RGPServer::RGPServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::RGP, RGP_SERVER_MIN_VERSION, RGP_SERVER_MAX_VERSION)
            , m_traceStatus(TraceStatus::Idle)
            , m_pCurrentSessionData(nullptr)
            , m_profilingStatus(ProfilingStatus::NotAvailable)
            , m_spmCounterData(pMsgChannel->GetAllocCb())
            , m_spmValidationCb({ nullptr, nullptr })
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
            memset(&m_traceParameters, 0, sizeof(m_traceParameters));
        }

        RGPServer::~RGPServer()
        {
        }

        void RGPServer::Finalize()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            BaseProtocolServer::Finalize();
        }

        bool RGPServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void RGPServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate session data for the newly established session
            RGPSession* pSessionData = DD_NEW(RGPSession, m_pMsgChannel->GetAllocCb())(m_pMsgChannel->GetAllocCb());
            // WA: Force MSVC's static analyzer to ignore unhandled OOM.
            DD_ASSUME(pSessionData != nullptr);

            pSessionData->state = SessionState::ReceivePayload;
            memset(&pSessionData->payload, 0, sizeof(RGPPayload));

            pSession->SetUserData(pSessionData);
        }

        void RGPServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            RGPSession* pSessionData = reinterpret_cast<RGPSession*>(pSession->GetUserData());

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            switch (m_traceStatus)
            {
                case TraceStatus::Idle:
                {
                    // Process messages in when we're not executing a trace.
                    switch (pSessionData->state)
                    {
                        case SessionState::ReceivePayload:
                        {
                            uint32 bytesReceived = 0;
                            Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                            if (result == Result::Success)
                            {
                                DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);
                                pSessionData->state = SessionState::ProcessPayload;
                            }

                            break;
                        }

                        case SessionState::ProcessPayload:
                        {
                            switch (pSessionData->payload.command)
                            {
                                case RGPMessage::ExecuteTraceRequest:
                                {
                                    // We should always have a null session data pointer when in the idle trace state.
                                    DD_ASSERT(m_pCurrentSessionData == nullptr);

                                    if (m_profilingStatus == ProfilingStatus::Enabled)
                                    {
                                        if (pSession->GetVersion() < RGP_PROFILING_CLOCK_MODES_VERSION)
                                        {
                                            const TraceParameters& traceParameters = pSessionData->payload.executeTraceRequest.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if ((pSession->GetVersion() == RGP_PROFILING_CLOCK_MODES_VERSION) ||
                                                 (pSession->GetVersion() == RGP_TRACE_PROGRESS_VERSION))
                                        {
                                            const TraceParametersV2& traceParameters = pSessionData->payload.executeTraceRequestV2.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if (pSession->GetVersion() == RGP_COMPUTE_PRESENTS_VERSION)
                                        {
                                            const TraceParametersV3& traceParameters = pSessionData->payload.executeTraceRequestV3.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;
                                        }
                                        else if ((pSession->GetVersion() == RGP_TRIGGER_MARKERS_VERSION) ||
                                                 (pSession->GetVersion() == RGP_PENDING_ABORT_VERSION))
                                        {
                                            const TraceParametersV4& traceParameters = pSessionData->payload.executeTraceRequestV4.parameters;
                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;

                                            m_traceParameters.beginTag =
                                                ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) | traceParameters.beginTagLow);
                                            m_traceParameters.endTag =
                                                ((static_cast<uint64>(traceParameters.endTagHigh) << 32) | traceParameters.endTagLow);

                                            Platform::Strncpy(m_traceParameters.beginMarker,
                                                              traceParameters.beginMarker,
                                                              sizeof(m_traceParameters.beginMarker));

                                            Platform::Strncpy(m_traceParameters.endMarker,
                                                              traceParameters.endMarker,
                                                              sizeof(m_traceParameters.endMarker));
                                        }
                                        else if (pSession->GetVersion() == RGP_FRAME_CAPTURE_VERSION)
                                        {
                                            const TraceParametersV5& traceParameters = pSessionData->payload.executeTraceRequestV5.parameters;

                                            m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                            m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                            m_traceParameters.captureMode = traceParameters.captureMode;
                                            m_traceParameters.flags.u32All = traceParameters.flags.u32All;

                                            m_traceParameters.captureStartIndex = traceParameters.captureStartIndex;
                                            m_traceParameters.captureStopIndex  = traceParameters.captureStopIndex;

                                            m_traceParameters.beginTag =
                                                ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) | traceParameters.beginTagLow);
                                            m_traceParameters.endTag =
                                                ((static_cast<uint64>(traceParameters.endTagHigh) << 32) | traceParameters.endTagLow);

                                            Platform::Strncpy(m_traceParameters.beginMarker,
                                                              traceParameters.beginMarker,
                                                              sizeof(m_traceParameters.beginMarker));

                                            Platform::Strncpy(m_traceParameters.endMarker,
                                                              traceParameters.endMarker,
                                                              sizeof(m_traceParameters.endMarker));
                                        }
                                        else if (pSession->GetVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS)
                                        {
                                            // Nothing to do here since the trace parameters are now handled by UpdateTraceParameters
                                        }
                                        else
                                        {
                                            // Unhandled protocol version
                                            DD_UNREACHABLE();
                                        }

                                        m_traceStatus = TraceStatus::Pending;
                                        m_pCurrentSessionData = pSessionData;
                                        m_pCurrentSessionData->state = SessionState::TransferTraceData;
                                        m_pCurrentSessionData->version = pSession->GetVersion();
                                        m_pCurrentSessionData->traceSizeInBytes = 0;
                                    }
                                    else
                                    {
                                        if (pSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                                        {
                                            pSessionData->payload.command = RGPMessage::TraceDataHeader;
                                            pSessionData->payload.traceDataHeader.numChunks = 0;
                                            pSessionData->payload.traceDataHeader.sizeInBytes = 0;
                                            pSessionData->payload.traceDataHeader.result = Result::Error;
                                        }
                                        else
                                        {
                                            pSessionData->payload.command = RGPMessage::TraceDataSentinel;
                                            pSessionData->payload.traceDataSentinel.result = Result::Error;
                                        }

                                        pSessionData->state = SessionState::SendPayload;
                                    }

                                    break;
                                }

                                case RGPMessage::QueryProfilingStatusRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::QueryProfilingStatusResponse;

                                    const ProfilingStatus profilingStatus = m_profilingStatus;

                                    pSessionData->payload.queryProfilingStatusResponse.status = profilingStatus;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }

                                case RGPMessage::EnableProfilingRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::EnableProfilingResponse;

                                    Result result = Result::Error;

                                    // Profiling can only be enabled before the server is finalized
                                    if ((m_isFinalized == false) & (m_profilingStatus == ProfilingStatus::Available))
                                    {
                                        m_profilingStatus = ProfilingStatus::Enabled;
                                        result = Result::Success;
                                    }

                                    pSessionData->payload.enableProfilingStatusResponse.result = result;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }

                                case RGPMessage::QueryTraceParametersRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::QueryTraceParametersResponse;

                                    // Make sure our session version is new enough to use this interface.
                                    Result result =
                                        (pSession->GetVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS) ? Result::Success
                                                                                                   : Result::VersionMismatch;

                                    if (result == Result::Success)
                                    {
                                        if (pSession->GetVersion() == RGP_DECOUPLED_TRACE_PARAMETERS)
                                        {
                                            TraceParametersV6& parameters =
                                                pSessionData->payload.queryTraceParametersResponse.parameters;

                                            parameters.gpuMemoryLimitInMb = m_traceParameters.gpuMemoryLimitInMb;
                                            parameters.numPreparationFrames = m_traceParameters.numPreparationFrames;

                                            parameters.captureStartIndex = m_traceParameters.captureStartIndex;
                                            parameters.captureStopIndex = m_traceParameters.captureStopIndex;

                                            parameters.captureMode = m_traceParameters.captureMode;

                                            parameters.flags.u32All = m_traceParameters.flags.u32All;

                                            parameters.beginTagHigh = static_cast<uint32>(m_traceParameters.beginTag >> 32);
                                            parameters.beginTagLow = static_cast<uint32>(m_traceParameters.beginTag & 0xFFFFFFFF);
                                            parameters.endTagHigh = static_cast<uint32>(m_traceParameters.endTag >> 32);
                                            parameters.endTagLow = static_cast<uint32>(m_traceParameters.endTag & 0xFFFFFFFF);

                                            Platform::Strncpy(parameters.beginMarker,
                                                m_traceParameters.beginMarker,
                                                sizeof(parameters.beginMarker));

                                            Platform::Strncpy(parameters.endMarker,
                                                m_traceParameters.endMarker,
                                                sizeof(parameters.endMarker));

                                            parameters.pipelineHashHi =
                                                static_cast<uint32>(m_traceParameters.pipelineHash >> 32);
                                            parameters.pipelineHashLo =
                                                static_cast<uint32>(m_traceParameters.pipelineHash & 0xFFFFFFFF);
                                        }
                                        else
                                        {
                                            TraceParametersV7& parameters =
                                                pSessionData->payload.queryTraceParametersResponseV2.parameters;

                                            parameters.gpuMemoryLimitInMb = m_traceParameters.gpuMemoryLimitInMb;
                                            parameters.numPreparationFrames = m_traceParameters.numPreparationFrames;

                                            parameters.captureStartIndex = m_traceParameters.captureStartIndex;
                                            parameters.captureStopIndex = m_traceParameters.captureStopIndex;

                                            parameters.captureMode = m_traceParameters.captureMode;

                                            parameters.flags.u32All = m_traceParameters.flags.u32All;

                                            parameters.beginTagHigh = static_cast<uint32>(m_traceParameters.beginTag >> 32);
                                            parameters.beginTagLow = static_cast<uint32>(m_traceParameters.beginTag & 0xFFFFFFFF);
                                            parameters.endTagHigh = static_cast<uint32>(m_traceParameters.endTag >> 32);
                                            parameters.endTagLow = static_cast<uint32>(m_traceParameters.endTag & 0xFFFFFFFF);

                                            Platform::Strncpy(parameters.beginMarker,
                                                m_traceParameters.beginMarker,
                                                sizeof(parameters.beginMarker));

                                            Platform::Strncpy(parameters.endMarker,
                                                m_traceParameters.endMarker,
                                                sizeof(parameters.endMarker));

                                            parameters.pipelineHashHi =
                                                static_cast<uint32>(m_traceParameters.pipelineHash >> 32);
                                            parameters.pipelineHashLo =
                                                static_cast<uint32>(m_traceParameters.pipelineHash & 0xFFFFFFFF);

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
                                            parameters.seMask = m_traceParameters.seMask;
#endif
                                        }
                                    }

                                    pSessionData->payload.queryTraceParametersResponse.result = result;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }

                                case RGPMessage::UpdateTraceParametersRequest:
                                {
                                    pSessionData->payload.command = RGPMessage::UpdateTraceParametersResponse;

                                    // Make sure our session version is new enough to use this interface.
                                    Result result =
                                        (pSession->GetVersion() >= RGP_DECOUPLED_TRACE_PARAMETERS) ? Result::Success
                                                                                                   : Result::VersionMismatch;

                                    if (result == Result::Success)
                                    {
                                        // Trace parameters can only be updated if there's no trace in progress.
                                        if (m_traceStatus == TraceStatus::Idle)
                                        {
                                            if (pSession->GetVersion() == RGP_DECOUPLED_TRACE_PARAMETERS)
                                            {
                                                const TraceParametersV6& traceParameters =
                                                    pSessionData->payload.updateTraceParametersRequest.parameters;

                                                m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                                m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                                m_traceParameters.captureMode = traceParameters.captureMode;
                                                m_traceParameters.flags.u32All = traceParameters.flags.u32All;

                                                m_traceParameters.captureStartIndex = traceParameters.captureStartIndex;
                                                m_traceParameters.captureStopIndex = traceParameters.captureStopIndex;

                                                m_traceParameters.beginTag =
                                                    ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) |
                                                        traceParameters.beginTagLow);
                                                m_traceParameters.endTag =
                                                    ((static_cast<uint64>(traceParameters.endTagHigh) << 32) |
                                                        traceParameters.endTagLow);

                                                Platform::Strncpy(m_traceParameters.beginMarker,
                                                    traceParameters.beginMarker,
                                                    sizeof(m_traceParameters.beginMarker));

                                                Platform::Strncpy(m_traceParameters.endMarker,
                                                    traceParameters.endMarker,
                                                    sizeof(m_traceParameters.endMarker));

                                                m_traceParameters.pipelineHash =
                                                    ((static_cast<uint64>(traceParameters.pipelineHashHi) << 32) |
                                                        static_cast<uint64>(traceParameters.pipelineHashLo));
                                            }
                                            else
                                            {
                                                const TraceParametersV7& traceParameters =
                                                    pSessionData->payload.updateTraceParametersRequestV2.parameters;

                                                m_traceParameters.gpuMemoryLimitInMb = traceParameters.gpuMemoryLimitInMb;
                                                m_traceParameters.numPreparationFrames = traceParameters.numPreparationFrames;
                                                m_traceParameters.captureMode = traceParameters.captureMode;
                                                m_traceParameters.flags.u32All = traceParameters.flags.u32All;

                                                m_traceParameters.captureStartIndex = traceParameters.captureStartIndex;
                                                m_traceParameters.captureStopIndex = traceParameters.captureStopIndex;

                                                m_traceParameters.beginTag =
                                                    ((static_cast<uint64>(traceParameters.beginTagHigh) << 32) |
                                                        traceParameters.beginTagLow);
                                                m_traceParameters.endTag =
                                                    ((static_cast<uint64>(traceParameters.endTagHigh) << 32) |
                                                        traceParameters.endTagLow);

                                                Platform::Strncpy(m_traceParameters.beginMarker,
                                                    traceParameters.beginMarker,
                                                    sizeof(m_traceParameters.beginMarker));

                                                Platform::Strncpy(m_traceParameters.endMarker,
                                                    traceParameters.endMarker,
                                                    sizeof(m_traceParameters.endMarker));

                                                m_traceParameters.pipelineHash =
                                                    ((static_cast<uint64>(traceParameters.pipelineHashHi) << 32) |
                                                        static_cast<uint64>(traceParameters.pipelineHashLo));

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
                                                m_traceParameters.seMask = traceParameters.seMask;
#endif
                                            }

                                            result = Result::Success;
                                        }
                                        else
                                        {
                                            // Set the result to NotReady since we can handle this request eventually,
                                            // just not right now.
                                            result = Result::NotReady;
                                        }
                                    }

                                    pSessionData->payload.updateTraceParametersResponse.result = result;

                                    pSessionData->state = SessionState::SendPayload;

                                    break;
                                }
                                case RGPMessage::UpdateSpmConfigRequest:
                                {
                                    // Make sure our session version is new enough to use this interface.
                                    if (pSession->GetVersion() >= RGP_SPM_COUNTERS_VERSION)
                                    {
                                        // Perf counters can only be updated if there's no trace in progress.
                                        if (m_traceStatus == TraceStatus::Idle)
                                        {
                                            const UpdateSpmConfigRequestPayload& updateCountersRequest =
                                                pSessionData->payload.updateSpmConfigRequest;

                                            pSessionData->spmConfig.sampleFrequency = updateCountersRequest.sampleFrequency;
                                            pSessionData->spmConfig.memoryLimitInMb = updateCountersRequest.memoryLimitInMb;
                                            pSessionData->spmCounters.Clear();

                                            // If we have data payloads following this packet, then we need to transition into
                                            // a special transfer state in order to receive them.
                                            if (updateCountersRequest.numDataPayloads > 0)
                                            {
                                                pSessionData->numSpmUpdatePackets = updateCountersRequest.numDataPayloads;
                                                pSessionData->state = SessionState::TransferSpmConfigData;
                                            }
                                            else
                                            {
                                                // We don't have any counters to receive so we're done with the update
                                                const Result result =
                                                    UpdateSpmConfig(pSessionData->spmConfig, pSessionData->spmCounters);
                                                pSessionData->payload.command = RGPMessage::UpdateSpmConfigResponse;
                                                pSessionData->payload.updateSpmConfigResponse.result = result;
                                                pSessionData->state = SessionState::SendPayload;
                                            }
                                        }
                                        else
                                        {
                                            // Set the result to NotReady since we can handle this request eventually,
                                            // just not right now.
                                            pSessionData->payload.command = RGPMessage::UpdateSpmConfigResponse;
                                            pSessionData->payload.updateSpmConfigResponse.result = Result::NotReady;
                                            pSessionData->state = SessionState::SendPayload;
                                        }
                                    }
                                    else
                                    {
                                        pSessionData->payload.command = RGPMessage::UpdateSpmConfigResponse;
                                        pSessionData->payload.updateSpmConfigResponse.result = Result::VersionMismatch;
                                        pSessionData->state = SessionState::SendPayload;
                                    }

                                    break;
                                }
                                default:
                                {
                                    // Invalid command
                                    DD_UNREACHABLE();
                                    break;
                                }
                            }

                            break;
                        }

                        case SessionState::TransferSpmConfigData:
                        {
                            uint32 bytesReceived = 0;
                            Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                            if (result == Result::Success)
                            {
                                if (pSessionData->payload.command == RGPMessage::UpdateSpmConfigData)
                                {
                                    const UpdateSpmConfigDataPayload& updateCounterData =
                                            pSessionData->payload.updateSpmConfigData;

                                    // Copy all the counters into our local collection
                                    const uint32 numCounters = updateCounterData.numCounters;
                                    for (uint32 counterIndex = 0; counterIndex < numCounters; ++counterIndex)
                                    {
                                        const SpmCounterId& counter = updateCounterData.counters[counterIndex];

                                        ServerSpmCounterId serverCounter = {};
                                        serverCounter.blockId    = counter.blockId;
                                        serverCounter.instanceId = counter.instanceId;
                                        serverCounter.eventId    = counter.eventId;

                                        pSessionData->spmCounters.PushBack(serverCounter);
                                    }

                                    DD_ASSERT(pSessionData->numSpmUpdatePackets > 0);
                                    --pSessionData->numSpmUpdatePackets;

                                    // Once we receive all the config updates, transition back to the normal session state cycle
                                    if (pSessionData->numSpmUpdatePackets == 0)
                                    {
                                        result = UpdateSpmConfig(pSessionData->spmConfig, pSessionData->spmCounters);
                                        pSessionData->payload.command = RGPMessage::UpdateSpmConfigResponse;
                                        pSessionData->payload.updateSpmConfigResponse.result = result;
                                        pSessionData->state = SessionState::SendPayload;
                                    }
                                }
                                else
                                {
                                    DD_ASSERT_REASON("Received non config update packet in config update state!");
                                }
                            }

                            break;
                        }

                        case SessionState::SendPayload:
                        {
                            Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                            if (result == Result::Success)
                            {
                                pSessionData->state = SessionState::ReceivePayload;
                            }

                            break;
                        }

                        default:
                        {
                            DD_UNREACHABLE();
                            break;
                        }
                    }

                    break;
                }

                case TraceStatus::Pending:
                case TraceStatus::Running:
                case TraceStatus::Finishing:
                {
                    // We should never enter this state with a null session data pointer.
                    // The termination callback should prevent this from happening.
                    DD_ASSERT(m_pCurrentSessionData != nullptr);

                    // Make sure we only attempt to talk to the session that requested the trace.
                    if (m_pCurrentSessionData == pSessionData)
                    {
                        // The session should always be ready to transfer data in this state.
                        DD_ASSERT(m_pCurrentSessionData->state == SessionState::TransferTraceData);

                        // Look for an abort request if necessary.
                        // Aborts are only supporting in the pending state if we're at an appropriate protocol version.
                        if (((pSession->GetVersion() >= RGP_TRACE_PROGRESS_VERSION) & (!pSessionData->abortRequestedByClient)) &
                            ((m_traceStatus != TraceStatus::Pending) | (pSession->GetVersion() >= RGP_PENDING_ABORT_VERSION)))
                        {
                            uint32 bytesReceived = 0;
                            Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                            if (result == Result::Success)
                            {
                                DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);

                                if (pSessionData->payload.command == RGPMessage::AbortTrace)
                                {
                                    pSessionData->abortRequestedByClient = true;
                                }
                                else
                                {
                                    // We should only ever receive abort requests in this state.
                                    DD_WARN_ALWAYS();
                                }
                            }
                        }

                        // If the client requested an abort, then send the trace sentinel.
                        if (pSessionData->abortRequestedByClient)
                        {
                            m_pCurrentSessionData->payload.command = RGPMessage::TraceDataSentinel;
                            m_pCurrentSessionData->payload.traceDataSentinel.result = Result::Aborted;

                            Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                            if (result == Result::Success)
                            {
                                // The trace was aborted. Move back to idle and reset all state.
                                ClearCurrentSession();
                            }
                        }
                        else if ((m_traceStatus == TraceStatus::Running) | (m_traceStatus == TraceStatus::Finishing))
                        {
                            // We should only consider sending trace data if we're in the running or finishing states.

                            // When trace progress is supported, we only send data once the trace is completed.
                            // We should always in the finishing state in that scenario.
                            const bool sendTraceData = (pSession->GetVersion() >= RGP_TRACE_PROGRESS_VERSION) ? (m_traceStatus == TraceStatus::Finishing)
                                                                                                              : true;
                            if (sendTraceData)
                            {
                                Result result = Result::Success;

                                while (m_pCurrentSessionData->chunkPayloads.IsEmpty() == false)
                                {
                                    result = pSession->Send(sizeof(RGPPayload), m_pCurrentSessionData->chunkPayloads.PeekFront(), kNoWait);

                                    if (result == Result::Success)
                                    {
                                        m_pCurrentSessionData->chunkPayloads.PopFront();
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }

                                if ((result == Result::Success) &
                                    (m_traceStatus == TraceStatus::Finishing))
                                {
                                    // If we make it this far with a success result in the Finishing state, we've sent all the chunk data.
                                    ClearCurrentSession();
                                }
                            }
                        }
                    }

                    break;
                }

                case TraceStatus::Aborting:
                {
                    if (m_pCurrentSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                    {
                        m_pCurrentSessionData->payload.command = RGPMessage::TraceDataHeader;
                        m_pCurrentSessionData->payload.traceDataHeader.numChunks = 0;
                        m_pCurrentSessionData->payload.traceDataHeader.sizeInBytes = 0;
                        m_pCurrentSessionData->payload.traceDataHeader.result = Result::Error;
                    }
                    else
                    {
                        m_pCurrentSessionData->payload.command = RGPMessage::TraceDataSentinel;
                        m_pCurrentSessionData->payload.traceDataSentinel.result = Result::Error;
                    }

                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        // The trace aborted. Move back to idle and reset all state.
                        ClearCurrentSession();
                    }

                    break;
                }

                default:
                {
                    // Do nothing
                    break;
                }
            }
        }

        void RGPServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            RGPSession *pRGPSession = reinterpret_cast<RGPSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pRGPSession != nullptr)
            {
                Platform::LockGuard<Platform::Mutex> lock(m_mutex);

                if (m_pCurrentSessionData == pRGPSession)
                {
                    m_traceStatus = TraceStatus::Idle;

                    m_pCurrentSessionData = nullptr;
                }
                DD_DELETE(pRGPSession, m_pMsgChannel->GetAllocCb());
            }
        }

        bool RGPServer::TracesEnabled()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool tracesEnabled = (m_profilingStatus == ProfilingStatus::Enabled);

            return tracesEnabled;
        }

        Result RGPServer::EnableTraces()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure we're not currently running a trace and that traces are currently disabled.
            if ((m_traceStatus == TraceStatus::Idle) & (m_profilingStatus == ProfilingStatus::NotAvailable))
            {
                m_profilingStatus = ProfilingStatus::Available;
                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::DisableTraces()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure we're not currently running a trace.
            if (m_traceStatus == TraceStatus::Idle)
            {
                m_profilingStatus = ProfilingStatus::NotAvailable;
                result = Result::Success;
            }

            return result;
        }

        bool RGPServer::IsTracePending()
        {
            // Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool isTracePending = (m_traceStatus == TraceStatus::Pending);

            return isTracePending;
        }

        bool RGPServer::IsTraceRunning()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const bool isTraceRunning = (m_traceStatus == TraceStatus::Running);

            return isTraceRunning;
        }

        Result RGPServer::BeginTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // A trace can only begin if a client requests one.
            if (m_traceStatus == TraceStatus::Pending)
            {
                m_traceStatus = TraceStatus::Running;
                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::EndTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there was a trace running before.
            if (m_traceStatus == TraceStatus::Running)
            {
                if (m_pCurrentSessionData != nullptr)
                {
                    if (m_pCurrentSessionData->version >= RGP_TRACE_PROGRESS_VERSION)
                    {
                        // Inject the trace header
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateFront();
                        result = (pPayload != nullptr) ? Result::Success : Result::Error;

                        if (result == Result::Success)
                        {
                            // todo: Support trace sizes larger than 4GB
                            DD_ASSERT(m_pCurrentSessionData->traceSizeInBytes < (~0u));

                            pPayload->command = RGPMessage::TraceDataHeader;
                            pPayload->traceDataHeader.result = Result::Success;
                            pPayload->traceDataHeader.numChunks = static_cast<uint32>(m_pCurrentSessionData->chunkPayloads.Size() - 1);
                            pPayload->traceDataHeader.sizeInBytes = static_cast<uint32>(m_pCurrentSessionData->traceSizeInBytes);

                            pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                            result = (pPayload != nullptr) ? Result::Success : Result::Error;
                        }

                        if (result == Result::Success)
                        {
                            pPayload->command = RGPMessage::TraceDataSentinel;
                            pPayload->traceDataSentinel.result = Result::Success;

                            m_traceStatus = TraceStatus::Finishing;
                        }
                    }
                    else
                    {
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                        if (pPayload != nullptr)
                        {
                            m_traceStatus = TraceStatus::Finishing;
                            pPayload->command = RGPMessage::TraceDataSentinel;
                            pPayload->traceDataSentinel.result = Result::Success;
                            result = Result::Success;
                        }
                    }
                }
                else
                {
                    // The client that requested the trace has disconnected. Discard the trace.
                    m_traceStatus = TraceStatus::Idle;
                    result = Result::Success;
                }

            }

            return result;
        }

        Result RGPServer::AbortTrace()
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there was a trace running before.
            if (m_traceStatus == TraceStatus::Running)
            {
                if (m_pCurrentSessionData != nullptr)
                {
                    m_traceStatus = TraceStatus::Aborting;
                }
                else
                {
                    // The client that requested the trace has disconnected. Discard the trace.
                    m_traceStatus = TraceStatus::Idle;
                }

                result = Result::Success;
            }

            return result;
        }

        Result RGPServer::WriteTraceData(const uint8* pTraceData, size_t traceDataSize)
        {
            Result result = Result::Error;

            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            // Make sure there is a trace running.
            if (m_traceStatus == TraceStatus::Running)
            {
                size_t traceDataRemaining = traceDataSize;

                if (m_pCurrentSessionData != nullptr)
                {
                    m_pCurrentSessionData->traceSizeInBytes += traceDataSize;
                    while (traceDataRemaining > 0)
                    {
                        RGPPayload *pPayload = m_pCurrentSessionData->chunkPayloads.AllocateBack();
                        if (pPayload != nullptr)
                        {
                            const size_t dataSize = ((traceDataRemaining < kMaxTraceDataChunkSize) ? traceDataRemaining
                                                     : kMaxTraceDataChunkSize);

                            memcpy(pPayload->traceDataChunk.chunk.data, pTraceData, dataSize);
                            pPayload->traceDataChunk.chunk.dataSize = static_cast<uint32>(dataSize);
                            pPayload->command = RGPMessage::TraceDataChunk;

                            pTraceData += dataSize;
                            traceDataRemaining -= dataSize;
                        }
                        else
                        {
                            break;
                        }
                    }
                }

                if (traceDataRemaining == 0)
                    result = Result::Success;
            }

            return result;
        }

        ProfilingStatus RGPServer::QueryProfilingStatus()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const ProfilingStatus profilingStatus = m_profilingStatus;

            return profilingStatus;
        }

        ServerTraceParametersInfo RGPServer::QueryTraceParameters()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const ServerTraceParametersInfo traceParameters = m_traceParameters;

            return traceParameters;
        }

        Result RGPServer::QuerySpmConfig(ServerSpmConfig* pConfig, Vector<ServerSpmCounterId>* pCounterData)
        {
            Result result = Result::Success;

            if ((pConfig != nullptr) && (pCounterData != nullptr))
            {
                Platform::LockGuard<Platform::Mutex> lock(m_mutex);

                (*pConfig) = m_spmConfig;

                pCounterData->Clear();
                for (size_t counterIndex = 0; counterIndex < m_spmCounterData.Size(); ++counterIndex)
                {
                    pCounterData->PushBack(m_spmCounterData[counterIndex]);
                }
            }
            else
            {
                result = Result::InvalidParameter;
            }

            return result;
        }

        void RGPServer::SetSpmValidationCallback(
            const ValidateSpmCallbackInfo& callback)
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            m_spmValidationCb = callback;
        }

        void RGPServer::LockData()
        {
            m_mutex.Lock();
        }

        void RGPServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        void RGPServer::ClearCurrentSession()
        {
            if (m_pCurrentSessionData != nullptr)
            {
                // Move back to idle state and reset all state if we have a valid session.
                m_traceStatus = TraceStatus::Idle;
                m_pCurrentSessionData->state = SessionState::ReceivePayload;
                m_pCurrentSessionData->version = 0;
                m_pCurrentSessionData->traceSizeInBytes = 0;
                m_pCurrentSessionData->chunkPayloads.Clear();
                m_pCurrentSessionData->abortRequestedByClient = false;
                m_pCurrentSessionData = nullptr;
            }
        }

        Result RGPServer::UpdateSpmConfig(
            const ServerSpmConfig&            config,
            const Vector<ServerSpmCounterId>& counters)
        {
            // Assume the data is valid by default in case we don't have a validation callback set.
            Result result = Result::Success;

            if (m_spmValidationCb.pfnValidateSpmConfig != nullptr)
            {
                if (m_spmValidationCb.pfnValidateSpmConfig(m_spmValidationCb.pUserdata, &config, &counters) == false)
                {
                    result = Result::InvalidParameter;
                }
            }

            if (result == Result::Success)
            {
                // Commit the configuration changes to memory since we passed validation

                m_spmConfig = config;

                m_spmCounterData.Resize(counters.Size());
                for (size_t counterIndex = 0; counterIndex < counters.Size(); ++counterIndex)
                {
                    m_spmCounterData[counterIndex] = counters[counterIndex];
                }
            }

            return result;
        }
    }
} // DevDriver
