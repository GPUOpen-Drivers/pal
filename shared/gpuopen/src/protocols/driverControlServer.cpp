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

#include "protocols/driverControlServer.h"
#include "msgChannel.h"
#include <cstring>
#include "protocols/systemProtocols.h"

#define DRIVERCONTROL_SERVER_MIN_MAJOR_VERSION 1
#define DRIVERCONTROL_SERVER_MAX_MAJOR_VERSION 3

namespace DevDriver
{
    namespace DriverControlProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            StepDriver
        };

        struct DriverControlSession
        {
            SizedPayloadContainer   payloadContainer;
            SharedPointer<ISession> pSession;
            SessionState            state;

            explicit DriverControlSession(const SharedPointer<ISession>& pSession)
                : payloadContainer()
                , pSession(pSession)
                , state(SessionState::ReceivePayload)
            {
                memset(&payloadContainer, 0, sizeof(payloadContainer));
            }

            // Helper functions for working with SizedPayloadContainers and managing back-compat.
            Result SendPayload(uint32 timeoutInMs)
            {
                // If we're running an older driver control version, always write the fixed payload size. Otherwise, write the real size.
                const uint32 payloadSize =
                    (pSession->GetVersion() >= DRIVERCONTROL_QUERYCLIENTINFO_VERSION) ? payloadContainer.payloadSize
                                                                                      : kLegacyDriverControlPayloadSize;

                return pSession->Send(payloadSize, payloadContainer.payload, timeoutInMs);
            }

        };

        DriverControlServer::DriverControlServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::DriverControl, DRIVERCONTROL_SERVER_MIN_MAJOR_VERSION, DRIVERCONTROL_SERVER_MAX_MAJOR_VERSION)
            , m_driverStatus(DriverStatus::PlatformInit)
            , m_driverResumedEvent(true)
            , m_numGpus(0)
            , m_deviceClockCallbackInfo()
            , m_numSessions(0)
            , m_stepCounter(0)
            , m_initStepRequested(false)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);

            for (uint32 gpuIndex = 0; gpuIndex < kMaxNumGpus; ++gpuIndex)
            {
                m_deviceClockModes[gpuIndex] = DeviceClockMode::Default;
            }
        }

        DriverControlServer::~DriverControlServer()
        {
        }

        void DriverControlServer::Finalize()
        {
            // If the driver hasn't marked the beginning of device init we do so here
            if (m_driverStatus == DriverStatus::PlatformInit)
            {
                StartDeviceInit();
            }

            // Finalize is the end of Device EarlyInit, this is where we halt if if there is a request
            // to halt on Device init.
            HandleDriverHalt(kDefaultDriverStartTimeoutMs);

            LockData();
            BaseProtocolServer::Finalize();
            UnlockData();
        }

        bool DriverControlServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void DriverControlServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            // Allocate session data for the newly established session
            DriverControlSession* pSessionData = DD_NEW(DriverControlSession, m_pMsgChannel->GetAllocCb())(pSession);
            Platform::AtomicIncrement(&m_numSessions);
            pSession->SetUserData(pSessionData);
        }

        void DriverControlServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            DriverControlSession* pSessionData = reinterpret_cast<DriverControlSession*>(pSession->GetUserData());

            switch (pSessionData->state)
            {
                case SessionState::ReceivePayload:
                {
                    Result result = pSession->ReceivePayload(&pSessionData->payloadContainer, kNoWait);

                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ProcessPayload;
                    }
                    else
                    {
                        // We should only receive specific error codes here.
                        // Assert if we see an unexpected error code.
                        DD_ASSERT((result == Result::Error)    ||
                                  (result == Result::NotReady) ||
                                  (result == Result::EndOfStream));
                    }

                    break;
                }

                case SessionState::ProcessPayload:
                {
                    SizedPayloadContainer& container = pSessionData->payloadContainer;
                    switch (container.GetPayload<DriverControlHeader>().command)
                    {
                        case DriverControlMessage::PauseDriverRequest:
                        {
                            Result result = Result::Error;

                            // Only allow pausing if we're already in the running state.
                            if (m_driverStatus == DriverStatus::Running)
                            {
                                m_driverStatus = DriverStatus::Paused;
                                m_driverResumedEvent.Clear();
                                result = Result::Success;
                            }

                            container.CreatePayload<PauseDriverResponsePayload>(result);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::ResumeDriverRequest:
                        {
                            Result result = Result::Error;

                            // Allow resuming the driver from the initial "halted on {Device/Platform} init" states and
                            // from the regular paused state.
                            if ((m_driverStatus == DriverStatus::HaltedOnDeviceInit) ||
                                (m_driverStatus == DriverStatus::HaltedOnPlatformInit) ||
                                (m_driverStatus == DriverStatus::Paused))
                            {
                                // If we're resuming from the paused state, move to the running state, otherwise we're moving from
                                // one of the halt on start states to the next initialization phase.
                                switch(m_driverStatus)
                                {
                                case DriverStatus::HaltedOnDeviceInit:
                                    m_driverStatus = DriverStatus::LateDeviceInit;
                                    result = Result::Success;
                                    break;

                                case DriverStatus::HaltedOnPlatformInit:
                                    m_driverStatus = DriverStatus::EarlyDeviceInit;
                                    result = Result::Success;
                                    break;

                                case DriverStatus::Paused:
                                    m_driverStatus = DriverStatus::Running;
                                    result = Result::Success;
                                    break;

                                default:
                                    DD_ASSERT_ALWAYS();
                                    break;
                                }
                                m_driverResumedEvent.Signal();
                            }

                            container.CreatePayload<ResumeDriverResponsePayload>(result);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDeviceClockModeRequest:
                        {
                            Result result = Result::Error;
                            DeviceClockMode clockMode = DeviceClockMode::Unknown;

                            const auto& payload = container.GetPayload<QueryDeviceClockModeRequestPayload>();

                            LockData();

                            const uint32 gpuIndex = payload.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                clockMode = m_deviceClockModes[gpuIndex];
                                result = Result::Success;
                            }

                            UnlockData();

                            container.CreatePayload<QueryDeviceClockModeResponsePayload>(result, clockMode);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::SetDeviceClockModeRequest:
                        {
                            Result result = Result::Error;

                            const auto& payload = container.GetPayload<SetDeviceClockModeRequestPayload>();

                            LockData();

                            const uint32 gpuIndex = payload.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.setCallback != nullptr)
                                {
                                    const DeviceClockMode clockMode = payload.mode;

                                    result = m_deviceClockCallbackInfo.setCallback(gpuIndex,
                                                                                   clockMode,
                                                                                   m_deviceClockCallbackInfo.pUserdata);
                                    if (result == Result::Success)
                                    {
                                        // Update the current clock mode
                                        m_deviceClockModes[gpuIndex] = clockMode;
                                    }
                                }
                            }

                            UnlockData();

                            container.CreatePayload<SetDeviceClockModeResponsePayload>(result);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDeviceClockRequest:
                        {
                            Result result = Result::Error;

                            float gpuClock = 0.0f;
                            float memClock = 0.0f;

                            const auto& payload = container.GetPayload<QueryDeviceClockRequestPayload>();

                            LockData();

                            const uint32 gpuIndex = payload.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.queryClockCallback != nullptr)
                                {
                                    result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                                                                                          &gpuClock,
                                                                                          &memClock,
                                                                                          m_deviceClockCallbackInfo.pUserdata);
                                }
                            }

                            UnlockData();

                            container.CreatePayload<QueryDeviceClockResponsePayload>(result, gpuClock, memClock);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryMaxDeviceClockRequest:
                        {
                            Result result = Result::Error;

                            float maxGpuClock = 0.0f;
                            float maxMemClock = 0.0f;

                            const auto& payload = container.GetPayload<QueryMaxDeviceClockRequestPayload>();

                            LockData();

                            const uint32 gpuIndex = payload.gpuIndex;
                            if (gpuIndex < m_numGpus)
                            {
                                if (m_deviceClockCallbackInfo.queryMaxClockCallback != nullptr)
                                {
                                    result = m_deviceClockCallbackInfo.queryMaxClockCallback(gpuIndex,
                                                                                             &maxGpuClock,
                                                                                             &maxMemClock,
                                                                                             m_deviceClockCallbackInfo.pUserdata);
                                }
                            }

                            UnlockData();

                            container.CreatePayload<QueryMaxDeviceClockResponsePayload>(result, maxGpuClock, maxMemClock);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryNumGpusRequest:
                        {
                            Result result = Result::Success;

                            LockData();

                            const uint32 numGpus = m_numGpus;

                            UnlockData();

                            container.CreatePayload<QueryNumGpusResponsePayload>(result, numGpus);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }

                        case DriverControlMessage::QueryDriverStatusRequest:
                        {
                            LockData();

                            const DriverStatus driverStatus = m_driverStatus;

                            UnlockData();

                            DriverStatus status;

                            // On older versions, override EarlyInit and LateInit to the running state to maintain backwards compatibility.
                            if ((pSession->GetVersion() < DRIVERCONTROL_INITIALIZATION_STATUS_VERSION) &&
                                ((driverStatus == DriverStatus::EarlyDeviceInit) || (driverStatus == DriverStatus::LateDeviceInit)))
                            {
                                status = DriverStatus::Running;
                            }
                            else
                            {
                                status = driverStatus;
                            }

                            container.CreatePayload<QueryDriverStatusResponsePayload>(status);
                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case DriverControlMessage::StepDriverRequest:
                        {
                            const auto& payload = container.GetPayload<StepDriverRequestPayload>();

                            if (m_driverStatus == DriverStatus::Paused && m_stepCounter == 0)
                            {
                                int32 count = Platform::Max((int32)payload.count, 1);
                                Platform::AtomicAdd(&m_stepCounter, count);
                                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Stepping driver %i frames", m_stepCounter);
                                pSessionData->state = SessionState::StepDriver;
                                ResumeDriver();
                            }
                            else if ((m_driverStatus == DriverStatus::HaltedOnPlatformInit) || (m_driverStatus == DriverStatus::HaltedOnDeviceInit))
                            {
                                m_initStepRequested = true;
                                ResumeDriver();
                            }
                            else
                            {
                                container.CreatePayload<StepDriverResponsePayload>(Result::Error);
                                pSessionData->state = SessionState::SendPayload;
                            }
                            break;
                        }
                        case DriverControlMessage::QueryClientInfoRequest:
                        {
                            DD_ASSERT(pSession->GetVersion() >= DRIVERCONTROL_QUERYCLIENTINFO_VERSION);

                            container.CreatePayload<QueryClientInfoResponsePayload>(m_pMsgChannel->GetClientInfo());
                            pSessionData->state = SessionState::SendPayload;
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

                case SessionState::SendPayload:
                {
                    Result result = pSessionData->SendPayload(kNoWait);

                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                case SessionState::StepDriver:
                {
                    if (m_driverStatus == DriverStatus::Paused && m_stepCounter == 0)
                    {
                        pSessionData->payloadContainer.CreatePayload<StepDriverResponsePayload>(Result::Success);
                        pSessionData->state = SessionState::SendPayload;
                    }
                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
            }
        }

        void DriverControlServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            DriverControlSession *pDriverControlSession = reinterpret_cast<DriverControlSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pDriverControlSession != nullptr)
            {
                Platform::AtomicDecrement(&m_numSessions);
                DD_DELETE(pDriverControlSession, m_pMsgChannel->GetAllocCb());
            }
        }

        void DriverControlServer::WaitForDriverResume()
        {
            if ((m_driverStatus == DriverStatus::Running) & (m_stepCounter > 0))
            {
                long stepsRemaining = Platform::AtomicDecrement(&m_stepCounter);
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] %i frames remaining", stepsRemaining);
                if (stepsRemaining == 0)
                {
                    PauseDriver();
                }
            }

            if (m_driverStatus == DriverStatus::Paused)
            {
                Result waitResult = Result::NotReady;
                while (waitResult == Result::NotReady)
                {
                    if (m_numSessions == 0)
                    {
                        const ClientInfoStruct &clientInfo = m_pMsgChannel->GetClientInfo();
                        ClientMetadata filter = {};

                        m_pMsgChannel->Send(kBroadcastClientId,
                                            Protocol::System,
                                            static_cast<MessageCode>(SystemProtocol::SystemMessage::Halted),
                                            filter,
                                            sizeof(ClientInfoStruct),
                                            &clientInfo);
                        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Sent system halted message");
                    }
                    waitResult = m_driverResumedEvent.Wait(kBroadcastIntervalInMs);
                }
                DD_ASSERT(m_driverStatus == DriverStatus::Running);
            }
        }

        bool DriverControlServer::IsDriverInitialized() const
        {
            // The running and paused states can only be reached after the driver has fully initialized.
            return ((m_driverStatus == DriverStatus::Running) || (m_driverStatus == DriverStatus::Paused));
        }

        void DriverControlServer::FinishDriverInitialization()
        {
            if (m_driverStatus == DriverStatus::LateDeviceInit)
            {
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Driver initialization finished\n");
                m_driverStatus = DriverStatus::Running;

                if (m_initStepRequested)
                {
                    PauseDriver();
                    WaitForDriverResume();
                }
            }
        }

        DriverStatus DriverControlServer::QueryDriverStatus()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            const DriverStatus driverStatus = m_driverStatus;

            return driverStatus;
        }

        void DriverControlServer::PauseDriver()
        {
            if ((m_driverStatus == DriverStatus::Running) ||
                (m_driverStatus == DriverStatus::EarlyDeviceInit) ||
                (m_driverStatus == DriverStatus::PlatformInit))
            {
                switch(m_driverStatus)
                {
                case DriverStatus::Running:
                    m_driverStatus = DriverStatus::Paused;
                    break;

                case DriverStatus::EarlyDeviceInit:
                    m_driverStatus = DriverStatus::HaltedOnDeviceInit;
                    break;

                case DriverStatus::PlatformInit:
                    m_driverStatus = DriverStatus::HaltedOnPlatformInit;
                    break;

                default:
                    DD_ASSERT_ALWAYS();
                    break;
                }
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Paused driver\n");
                m_driverResumedEvent.Clear();
            }
        }

        void DriverControlServer::ResumeDriver()
        {
            if ((m_driverStatus == DriverStatus::Paused) ||
                (m_driverStatus == DriverStatus::HaltedOnDeviceInit) ||
                (m_driverStatus == DriverStatus::HaltedOnPlatformInit))
            {
                switch (m_driverStatus)
                {
                case DriverStatus::HaltedOnDeviceInit:
                    m_driverStatus = DriverStatus::LateDeviceInit;
                    break;

                case DriverStatus::HaltedOnPlatformInit:
                    m_driverStatus = DriverStatus::EarlyDeviceInit;
                    break;

                case DriverStatus::Paused:
                    m_driverStatus = DriverStatus::Running;
                    break;

                default:
                    DD_ASSERT_ALWAYS();
                    break;
                }
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Resumed driver\n");
                m_driverResumedEvent.Signal();
            }
        }

        void DriverControlServer::SetDeviceClockCallback(const DeviceClockCallbackInfo& deviceClockCallbackInfo)
        {
            LockData();

            m_deviceClockCallbackInfo = deviceClockCallbackInfo;

            UnlockData();
        }

        void DriverControlServer::SetNumGpus(uint32 numGpus)
        {
            // Make sure the new number is less than or equal to the max.
            DD_ASSERT(numGpus <= kMaxNumGpus);

            LockData();

            m_numGpus = numGpus;

            UnlockData();
        }

        uint32 DriverControlServer::GetNumGpus()
        {
            LockData();

            uint32 numGpus = m_numGpus;

            UnlockData();

            return numGpus;
        }

        DeviceClockMode DriverControlServer::GetDeviceClockMode(uint32 gpuIndex)
        {
            LockData();

            DD_ASSERT(gpuIndex < m_numGpus);

            DeviceClockMode clockMode = m_deviceClockModes[gpuIndex];

            UnlockData();

            return clockMode;
        }

        void DriverControlServer::LockData()
        {
            m_mutex.Lock();
        }

        void DriverControlServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        void DriverControlServer::StartDeviceInit()
        {
            // This is the end of the PlatformInit phase, where we halt if if there is a request to halt on Platform init.
            HandleDriverHalt(kDefaultDriverStartTimeoutMs);
        }

        void DriverControlServer::HandleDriverHalt(uint64 timeoutInMs)
        {
            ClientId clientId = kBroadcastClientId;

            if (m_driverStatus == DriverStatus::EarlyDeviceInit)
            {
                if (m_initStepRequested)
                {
                    // If we've been asked to step from the previous state, then no need to look for a client status flag,
                    // just halt here
                    m_initStepRequested = false;
                    PauseDriver();
                }
                else
                {
                    ClientMetadata filter = {};
                    filter.status |= static_cast<StatusFlags>(ClientStatusFlags::DeviceHaltOnConnect);
                    if (m_pMsgChannel->FindFirstClient(filter, &clientId, kBroadcastIntervalInMs) == Result::Success)
                    {
                        DD_ASSERT(clientId != kBroadcastClientId);
                        DD_PRINT(LogLevel::Verbose,
                            "[DriverControlServer] Found client requesting driver halt on Device init: %u", clientId);
                        PauseDriver();
                    }
                }
            }

            if (m_driverStatus == DriverStatus::PlatformInit)
            {
                ClientMetadata filter = {};
                filter.status |= static_cast<StatusFlags>(ClientStatusFlags::PlatformHaltOnConnect);
                if (m_pMsgChannel->FindFirstClient(filter, &clientId, kBroadcastIntervalInMs) == Result::Success)
                {
                    DD_ASSERT(clientId != kBroadcastClientId);
                    DD_PRINT(LogLevel::Verbose,
                        "[DriverControlServer] Found client requesting driver halt on Platform init: %u", clientId);
                    PauseDriver();
                }
            }

            if ((m_driverStatus == DriverStatus::HaltedOnDeviceInit) || (m_driverStatus == DriverStatus::HaltedOnPlatformInit))
            {
                Result waitResult = Result::NotReady;
                uint64 startTime = Platform::GetCurrentTimeInMs();
                uint64 currentTime = startTime;

                while (waitResult == Result::NotReady)
                {
                    if (m_numSessions == 0)
                    {
                        if ((currentTime - startTime) > timeoutInMs)
                        {
                            ResumeDriver();
                            break;
                        }

                        const ClientInfoStruct &clientInfo = m_pMsgChannel->GetClientInfo();
                        ClientMetadata filter = {};

                        m_pMsgChannel->Send(clientId,
                                            Protocol::System,
                                            static_cast<MessageCode>(SystemProtocol::SystemMessage::Halted),
                                            filter,
                                            sizeof(ClientInfoStruct),
                                            &clientInfo);

                        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Sent system halted message");
                    }
                    waitResult = m_driverResumedEvent.Wait(kBroadcastIntervalInMs);
                    currentTime = Platform::GetCurrentTimeInMs();
                }
            }
            else
            {
                DD_ASSERT((m_driverStatus == DriverStatus::EarlyDeviceInit) || (m_driverStatus == DriverStatus::PlatformInit));
                // If we don't need to halt on start, just skip to the next init phase.
                m_driverStatus = (m_driverStatus == DriverStatus::EarlyDeviceInit) ?
                    DriverStatus::LateDeviceInit : DriverStatus::EarlyDeviceInit;
            }
        }
    }
} // DevDriver
