/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "baseProtocolServer.h"
#include "util/vector.h"

#include "rgpProtocol.h"

namespace DevDriver
{
    namespace RGPProtocol
    {
        enum class TraceStatus : uint32
        {
            Idle = 0,
            Pending,
            Running,
            Finishing,
            Aborting
        };

        struct ServerTraceParametersInfo
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

            uint64 pipelineHash;

#if DD_VERSION_SUPPORTS(GPUOPEN_RGP_SPM_COUNTERS_VERSION)
            uint32 seMask;
#endif
        };

        struct ServerSpmCounterId
        {
            uint32 blockId;
            uint32 instanceId;
            uint32 eventId;
        };

        struct ServerSpmConfig
        {
            uint32 sampleFrequency;
            uint32 memoryLimitInMb;
        };

        typedef bool (*PFN_ValidateSpmConfig)(void* pUserdata, const ServerSpmConfig* pConfig, const Vector<ServerSpmCounterId>* pCounterData);
        struct ValidateSpmCallbackInfo
        {
            void*                 pUserdata;
            PFN_ValidateSpmConfig pfnValidateSpmConfig;
        };

        struct RGPSession;

        class RGPServer : public BaseProtocolServer
        {
        public:
            explicit RGPServer(IMsgChannel* pMsgChannel);
            ~RGPServer();

            void Finalize() override;

            bool AcceptSession(const SharedPointer<ISession>& pSession) override;
            void SessionEstablished(const SharedPointer<ISession>& pSession) override;
            void UpdateSession(const SharedPointer<ISession>& pSession) override;
            void SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason) override;

            // Returns true if traces are currently enabled.
            bool TracesEnabled();

            // Allows remote clients to request traces.
            Result EnableTraces();

            // Disable support for traces.
            Result DisableTraces();

            // Returns true if a client has requested a trace and it has not been started yet.
            bool IsTracePending();

            // Returns true if a client has requested a trace and it is currently running.
            bool IsTraceRunning();

            // Starts a new trace. This will only succeed if a trace was previously pending.
            Result BeginTrace();

            // Ends a trace. This will only succeed if a trace was previously in progress.
            Result EndTrace();

            // Aborts a trace. This will only succeed if a trace was previously in progress.
            Result AbortTrace();

            // Writes data into the current trace. This can only be performed when there is a trace in progress.
            Result WriteTraceData(const uint8* pTraceData, size_t traceDataSize);

            // Returns the current profiling status on the rgp server.
            ProfilingStatus QueryProfilingStatus();

            // Returns the current trace parameters on the rgp server.
            ServerTraceParametersInfo QueryTraceParameters();

            // Populates the provided structure with the current perf counter config and returns data for each counter
            // in the provided vector
            Result QuerySpmConfig(ServerSpmConfig* pConfig, Vector<ServerSpmCounterId>* pCounterData);

            // Sets a validation callback that will be used to validate SPM configuration data
            void SetSpmValidationCallback(const ValidateSpmCallbackInfo& callback);

        private:
            void LockData();
            void UnlockData();
            void ClearCurrentSession();
            Result UpdateSpmConfig(const ServerSpmConfig& config, const Vector<ServerSpmCounterId>& counters);

            Platform::Mutex            m_mutex;
            TraceStatus                m_traceStatus;
            RGPSession*                m_pCurrentSessionData;
            ProfilingStatus            m_profilingStatus;
            ServerTraceParametersInfo  m_traceParameters;
            ServerSpmConfig            m_spmConfig;
            Vector<ServerSpmCounterId> m_spmCounterData;
            ValidateSpmCallbackInfo    m_spmValidationCb;
        };
    }
} // DevDriver
