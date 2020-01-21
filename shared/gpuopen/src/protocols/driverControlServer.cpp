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

#include "protocols/driverControlServer.h"
#include "msgChannel.h"
#include "protocols/systemProtocols.h"

#define DRIVERCONTROL_SERVER_MIN_VERSION 1

// Gate the maximum version of the protocol based on the gpuopen interface version.
// Protocol versions beyond DRIVERCONTROL_DRIVER_INTERFACE_CLEANUP_VERSION require special server side support
// which is only available via the new gpuopen interface.
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
#define DRIVERCONTROL_SERVER_MAX_VERSION DRIVERCONTROL_PROTOCOL_VERSION
#else
#define DRIVERCONTROL_SERVER_MAX_VERSION DRIVERCONTROL_DRIVER_INTERFACE_CLEANUP_VERSION
#endif

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
    : BaseProtocolServer(pMsgChannel, Protocol::DriverControl, DRIVERCONTROL_SERVER_MIN_VERSION, DRIVERCONTROL_SERVER_MAX_VERSION)
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
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_CLEANUP_VERSION
    // Prior to this version clients called Finalize to move to LateDeviceInit, with the cleanup StartLateDeviceInit should be
    // called intead. To simulate that and preserve back-compat we'll check the status and just call StartLateDeviceInit() if
    // we're not yet in the LateDeviceInit state
    if (m_driverStatus == DriverStatus::EarlyDeviceInit)
    {
        StartLateDeviceInit();
    }
#endif

    BaseProtocolServer::Finalize();
}

//////////////// Session Handling Functions ////////////////////////
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
                    pSessionData->state = HandlePauseDriverRequest(container);
                    break;
                case DriverControlMessage::ResumeDriverRequest:
                    pSessionData->state = HandleResumeDriverRequest(container);
                    break;
                case DriverControlMessage::QueryDeviceClockModeRequest:
                    pSessionData->state = HandleQueryDeviceClockModeRequest(container);
                    break;
                case DriverControlMessage::SetDeviceClockModeRequest:
                    pSessionData->state = HandleSetDeviceClockModeRequest(container);
                    break;
                case DriverControlMessage::QueryDeviceClockRequest:
                    pSessionData->state = HandleQueryDeviceClockRequest(container);
                    break;
                case DriverControlMessage::QueryMaxDeviceClockRequest:
                    pSessionData->state = HandleQueryMaxDeviceClockRequest(container);
                    break;
                case DriverControlMessage::QueryNumGpusRequest:
                    pSessionData->state = HandleQueryNumGpusRequest(container);
                    break;
                case DriverControlMessage::QueryDriverStatusRequest:
                    pSessionData->state = HandleQueryDriverStatusRequest(container, pSession->GetVersion());
                    break;
                case DriverControlMessage::StepDriverRequest:
                    pSessionData->state = HandleStepDriverRequest(container);
                    break;
                case DriverControlMessage::QueryClientInfoRequest:
                    DD_ASSERT(pSession->GetVersion() >= DRIVERCONTROL_QUERYCLIENTINFO_VERSION);
                    container.CreatePayload<QueryClientInfoResponsePayload>(m_pMsgChannel->GetClientInfo());
                    pSessionData->state = SessionState::SendPayload;
                    break;
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
                case DriverControlMessage::QueryDeviceClockByModeRequest:
                    pSessionData->state = HandleQueryDeviceClockByModeRequest(container);
                    break;
#endif
                default:
                    DD_UNREACHABLE();
                    break;
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
            LockData();
            // We send a response to the StepDriverRequest once the stepping is complete.
            //   * During driver initialization we have completed stepping when we get to the next Halted state and there
            //       is no pending init step
            //   * After driver initialization, we have completed stepping when the step counter reaches zero
            if ((IsHalted() && (m_initStepRequested == false)) ||
                (IsDriverInitialized() && (m_stepCounter == 0)))
            {
                pSessionData->payloadContainer.CreatePayload<StepDriverResponsePayload>(Result::Success);
                pSessionData->state = SessionState::SendPayload;
            }
            UnlockData();
            break;
        }

        default:
        {
            DD_ASSERT_ALWAYS();
            break;
        }
    }
}

void DriverControlServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
{
    DD_UNUSED(terminationReason);
    DriverControlSession* pDriverControlSession = reinterpret_cast<DriverControlSession*>(pSession->SetUserData(nullptr));

    // Free the session data
    if (pDriverControlSession != nullptr)
    {
        Platform::AtomicDecrement(&m_numSessions);
        DD_DELETE(pDriverControlSession, m_pMsgChannel->GetAllocCb());
    }
}

//////////////// Protocol Message Handlers //////////////////////
SessionState DriverControlServer::HandlePauseDriverRequest(
    SizedPayloadContainer& container)
{
    Result result = Result::Error;

    // Only allow pausing if we're already in the running state.
    if (m_driverStatus == DriverStatus::Running)
    {
        PauseDriver();
        result = Result::Success;
    }

    container.CreatePayload<PauseDriverResponsePayload>(result);
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleResumeDriverRequest(
    SizedPayloadContainer& container)
{
    Result result = Result::Error;

    // Allow resuming the driver from the initial "halted on {Device/Platform} init" states and
    // from the regular paused state.
    if (IsHalted() || (m_driverStatus == DriverStatus::Paused))
    {
        // If we're resuming from the paused state, let ResumeDriver handle it, otherwise we're moving from
        // one of the halt on start states to the next initialization phase.
        switch (m_driverStatus)
        {
        case DriverStatus::HaltedOnDeviceInit:
        case DriverStatus::HaltedOnPlatformInit:
            m_driverResumedEvent.Signal();
            result = Result::Success;
            break;

        case DriverStatus::HaltedPostDeviceInit:
        case DriverStatus::Paused:
            ResumeDriver();
            result = Result::Success;
            break;

        default:
            DD_ASSERT_ALWAYS();
            break;
        }
    }

    container.CreatePayload<ResumeDriverResponsePayload>(result);
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleQueryDeviceClockModeRequest(
    SizedPayloadContainer& container)
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
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleSetDeviceClockModeRequest(
    SizedPayloadContainer& container)
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
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleQueryDeviceClockRequest(
    SizedPayloadContainer& container)
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
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
            result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                &gpuClock,
                &memClock,
                m_deviceClockCallbackInfo.pUserdata);
#else
            result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                DeviceClockMode::Default,
                &gpuClock,
                &memClock,
                m_deviceClockCallbackInfo.pUserdata);
#endif
        }
    }

    UnlockData();

    container.CreatePayload<QueryDeviceClockResponsePayload>(result, gpuClock, memClock);
    return SessionState::SendPayload;
}

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
SessionState DriverControlServer::HandleQueryDeviceClockByModeRequest(
    SizedPayloadContainer& container)
{
    Result result = Result::Error;

    float gpuClock = 0.0f;
    float memClock = 0.0f;

    const auto& payload = container.GetPayload<QueryDeviceClockByModeRequestPayload>();

    LockData();

    const uint32 gpuIndex = payload.gpuIndex;
    if (gpuIndex < m_numGpus)
    {
        if (m_deviceClockCallbackInfo.queryClockCallback != nullptr)
        {
            result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                payload.deviceClockMode,
                &gpuClock,
                &memClock,
                m_deviceClockCallbackInfo.pUserdata);
        }
    }

    UnlockData();

    container.CreatePayload<QueryDeviceClockByModeResponsePayload>(result, gpuClock, memClock);

    return SessionState::SendPayload;
}
#endif

SessionState DriverControlServer::HandleQueryMaxDeviceClockRequest(
    SizedPayloadContainer& container)
{
    Result result = Result::Error;

    float maxGpuClock = 0.0f;
    float maxMemClock = 0.0f;

    const auto& payload = container.GetPayload<QueryMaxDeviceClockRequestPayload>();

    LockData();

    const uint32 gpuIndex = payload.gpuIndex;
    if (gpuIndex < m_numGpus)
    {
        if (m_deviceClockCallbackInfo.queryClockCallback != nullptr)
        {
#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION >= GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION
            result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                DeviceClockMode::Peak,
                &maxGpuClock,
                &maxMemClock,
                m_deviceClockCallbackInfo.pUserdata);
#else
            result = m_deviceClockCallbackInfo.queryClockCallback(gpuIndex,
                &maxGpuClock,
                &maxMemClock,
                m_deviceClockCallbackInfo.pUserdata);
#endif
        }
    }

    UnlockData();

    container.CreatePayload<QueryMaxDeviceClockResponsePayload>(result, maxGpuClock, maxMemClock);
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleQueryNumGpusRequest(
    SizedPayloadContainer& container)
{
    LockData();

    const uint32 numGpus = m_numGpus;

    UnlockData();

    container.CreatePayload<QueryNumGpusResponsePayload>(Result::Success, numGpus);
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleQueryDriverStatusRequest(
    SizedPayloadContainer& container,
    const Version          sessionVersion)
{
    LockData();

    const DriverStatus driverStatus = m_driverStatus;

    UnlockData();

    DriverStatus status;

    // On older versions, override EarlyInit and LateInit to the running state to maintain backwards compatibility.
    if ((sessionVersion < DRIVERCONTROL_INITIALIZATION_STATUS_VERSION) &&
        ((driverStatus == DriverStatus::EarlyDeviceInit) || (driverStatus == DriverStatus::LateDeviceInit)))
    {
        status = DriverStatus::Running;
    }
    else if ((sessionVersion < DRIVERCONTROL_HALTEDPOSTDEVICEINIT_VERSION) &&
        (driverStatus == DriverStatus::HaltedPostDeviceInit))
    {
        // Override HaltedPostDeviceInit to Paused to support older clients
        status = DriverStatus::Paused;
    }
    else
    {
        status = driverStatus;
    }

    container.CreatePayload<QueryDriverStatusResponsePayload>(status);
    return SessionState::SendPayload;
}

SessionState DriverControlServer::HandleStepDriverRequest(
    SizedPayloadContainer& container)
{
    const auto& payload = container.GetPayload<StepDriverRequestPayload>();

    SessionState retState = SessionState::StepDriver;

    LockData();
    // If we're in either Paused or HaltedPostDeviceInit states and the step counter is zero
    if (((m_driverStatus == DriverStatus::Paused) ||
        (m_driverStatus == DriverStatus::HaltedPostDeviceInit)) &&
        m_stepCounter == 0)
    {
        int32 count = Platform::Max((int32)payload.count, 1);
        Platform::AtomicAdd(&m_stepCounter, count);
        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Stepping driver %i frames", m_stepCounter);
        // The StepDriverResponse will be sent in the handler for the StepDriver session state once
        // we reach the Paused state after stepping the requested number of frames.
        ResumeDriver();
    }
    else if ((m_driverStatus == DriverStatus::HaltedOnPlatformInit) || (m_driverStatus == DriverStatus::HaltedOnDeviceInit))
    {
        m_initStepRequested = true;
        // We send the StepDriverResponse will be sent in the handler for the StepDriver session state once
        // we reach the next Halted state. For now just signal the event to let the driver continue.
        m_driverResumedEvent.Signal();
    }
    else
    {
        container.CreatePayload<StepDriverResponsePayload>(Result::Error);
        retState = SessionState::SendPayload;
    }
    UnlockData();

    return retState;
}

//////////////// Driver State Functions ////////////////////////

void DriverControlServer::StartEarlyDeviceInit()
{
    DD_ASSERT(m_driverStatus == DriverStatus::PlatformInit);

    // This is the end of the PlatformInit phase, where we halt if if there is a request to halt on Platform init.
    DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Driver starting early device initialization\n");
    AdvanceDriverInitState();
}

void DriverControlServer::StartLateDeviceInit()
{
    DD_ASSERT(m_driverStatus == DriverStatus::EarlyDeviceInit);

    DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Driver starting late device initialization\n");
    AdvanceDriverInitState();
}

void DriverControlServer::FinishDeviceInit()
{
    DD_ASSERT(m_driverStatus == DriverStatus::LateDeviceInit);

    DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Driver device initialization finished\n");
    AdvanceDriverInitState();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Advances to the next driver initialization state, checking to see if the driver should pause/halt based on client
// StepDriver or StatusFlag requests.
void DriverControlServer::AdvanceDriverInitState()
{
    // Store the current driver state, since it may be updated when we check for halting
    DriverStatus currDriverState = m_driverStatus;

    // Handle the halted state, if necessary. If a Halt is requested this call will block until we receive a
    // StepDriver or ResumeDriver message from the client (or we timeout).
    HandleDriverHalt();

    // Then advance to the next state
    switch (currDriverState)
    {
    case DriverStatus::PlatformInit:
        m_driverStatus = DriverStatus::EarlyDeviceInit;
        break;
    case DriverStatus::EarlyDeviceInit:
        m_driverStatus = DriverStatus::LateDeviceInit;
        break;
    case DriverStatus::LateDeviceInit:
        m_driverStatus = DriverStatus::Running;
        break;
    default:
        // We should never never expect any other state here.
        DD_UNREACHABLE();
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Checks to see if the driver should halt.  If so, moves to the correct halted driver state based on the current state
void DriverControlServer::HandleDriverHalt()
{
    DD_ASSERT(IsDriverInitialized() == false);

    // Check if we should halt
    if (DiscoverHaltRequests())
    {
        // If so, update to the correct halted state
        switch (m_driverStatus)
        {
        case DriverStatus::PlatformInit:
            m_driverStatus = DriverStatus::HaltedOnPlatformInit;
            break;
        case DriverStatus::EarlyDeviceInit:
            m_driverStatus = DriverStatus::HaltedOnDeviceInit;
            break;
        case DriverStatus::LateDeviceInit:
            m_driverStatus = DriverStatus::HaltedPostDeviceInit;
            break;
        default:
            DD_UNREACHABLE();
            break;
        }
        // Clear the resume event then wait for resume
        m_driverResumedEvent.Clear();
        LockData();
        m_initStepRequested = false;
        UnlockData();
        WaitForResume();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Determines if the driver should halt by checking if there is a step request or a Halt status flag for the current
// state. Returns true if there is a pending halt request, false otherwise.
bool DriverControlServer::DiscoverHaltRequests()
{
    // This function should only be called during initialization
    DD_ASSERT(IsDriverInitialized() == false);

    LockData();
    bool ret = m_initStepRequested;
    UnlockData();

    // Since it is expensive, we only call FindFirstClient if we don't already have an active connection.
    if (m_initStepRequested == false)
    {
        if ((m_driverStatus == DriverStatus::PlatformInit) || (m_driverStatus == DriverStatus::EarlyDeviceInit))
        {
            ClientMetadata filter = {};
            if (m_driverStatus == DriverStatus::PlatformInit)
            {
                filter.status |= static_cast<StatusFlags>(ClientStatusFlags::PlatformHaltOnConnect);
            }
            else
            {
                filter.status |= static_cast<StatusFlags>(ClientStatusFlags::DeviceHaltOnConnect);
            }

            ClientId clientId = kBroadcastClientId;
            if (m_pMsgChannel->FindFirstClient(filter, &clientId, kBroadcastIntervalInMs) == Result::Success)
            {
                DD_ASSERT(clientId != kBroadcastClientId);
                DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Found client requesting driver halt on init: %u", clientId);
                ret = true;
            }
        }
    }

    return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Updates the state and clears the driver resume event which will cause the driver to pause the next time
// WaitForResume is called.
void DriverControlServer::PauseDriver()
{
    DD_ASSERT(m_driverStatus == DriverStatus::Running);

    m_driverStatus = DriverStatus::Paused;
    m_driverResumedEvent.Clear();
    DD_PRINT(LogLevel::Verbose, "[DriverControlServer] Paused driver\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Updates the state and signals the driver resume event which triggers WaitForResume to stop waiting and resume.
void DriverControlServer::ResumeDriver()
{
    // This function should only be called after initialization is complete, and the driver should be Paused/Halted.
    DD_ASSERT(IsDriverInitialized() && (m_driverStatus != DriverStatus::Running));

    m_driverStatus = DriverStatus::Running;
    m_driverResumedEvent.Signal();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This function is called by the driver every driver tick.  A driver tick is a periodic period defined by the driver,
// which for most drivers means every frame present. This call is where we will block for StepDriver or PauseDriver
// client requests.
void DriverControlServer::DriverTick()
{
    // First update the step counter if there's a pending StepDriver request.
    LockData();
    if (IsDriverInitialized() && (m_stepCounter > 0))
    {
        --m_stepCounter;
        DD_PRINT(LogLevel::Verbose, "[DriverControlServer] %i frames remaining", m_stepCounter);
        // If the step counter reaches zero then pause the driver
        if (m_stepCounter == 0)
        {
            PauseDriver();
        }
    }
    UnlockData();

    // If we're paused, then block waiting for a ResumeDriver or StepDriver request from the client (or timeout).
    if (m_driverStatus == DriverStatus::Paused)
    {
        WaitForResume();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This function handles the waiting during driver Pause/Halt. This function will block until a ResumeDriver message
// is received from the connected client, or until a timeout occurs if there is no client connection or it is lost.
void DriverControlServer::WaitForResume()
{
    // This function should only be called if we're already Halted/Paused
    DD_ASSERT(IsHalted() || (m_driverStatus == DriverStatus::Paused));

    Result waitResult = Result::NotReady;
    uint64 startTime = Platform::GetCurrentTimeInMs();
    const uint64 timeoutInMs = kDefaultDriverStartTimeoutMs;

    while (waitResult == Result::NotReady)
    {
        // If there is no client connection, we emit Halted system messages until we timeout to let interested clients
        // know that we're waiting and available.
        if (m_numSessions == 0)
        {
            if ((Platform::GetCurrentTimeInMs() - startTime) > timeoutInMs)
            {
                break;
            }

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
        else
        {
            // We don't want to start running the clock on a timeout until we lose connection, so requery
            // the start time
            startTime = Platform::GetCurrentTimeInMs();
        }

        waitResult = m_driverResumedEvent.Wait(kBroadcastIntervalInMs);
    }
}

//////////////// Other Public Functions ////////////////////////
bool DriverControlServer::IsDriverInitialized() const
{
    // Running, Paused and HaltedPostDeviceInit all indicate the driver is initialized.
    return ((m_driverStatus == DriverStatus::HaltedPostDeviceInit) ||
            (m_driverStatus == DriverStatus::Running) ||
            (m_driverStatus == DriverStatus::Paused));
}

DriverStatus DriverControlServer::QueryDriverStatus()
{
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);
    return m_driverStatus;
}

void DriverControlServer::SetDeviceClockCallback(const DeviceClockCallbackInfo& deviceClockCallbackInfo)
{
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);
    m_deviceClockCallbackInfo = deviceClockCallbackInfo;
}

void DriverControlServer::SetNumGpus(uint32 numGpus)
{
    // Make sure the new number is less than or equal to the max.
    if (numGpus <= kMaxNumGpus)
    {
        Platform::LockGuard<Platform::Mutex> lock(m_mutex);
        m_numGpus = numGpus;
    }
    else
    {
        DD_ASSERT_REASON("Received invalid numGpus in SetNumGpus()");
    }
}

uint32 DriverControlServer::GetNumGpus()
{
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);

    uint32 numGpus = m_numGpus;
    return numGpus;
}

DeviceClockMode DriverControlServer::GetDeviceClockMode(uint32 gpuIndex)
{
    Platform::LockGuard<Platform::Mutex> lock(m_mutex);

    DeviceClockMode clockMode = DeviceClockMode::Unknown;
    if(gpuIndex < m_numGpus)
    {
        clockMode = m_deviceClockModes[gpuIndex];
    }
    else
    {
        DD_ASSERT_REASON("Received invalid gpuIndex in GetDeviceClockMode()");
    }
    return clockMode;
}

//////////////// Helper Functions ////////////////////////
void DriverControlServer::LockData()
{
    m_mutex.Lock();
}

void DriverControlServer::UnlockData()
{
    m_mutex.Unlock();
}

} // namespace DriverControlProtocol
} // namespace DevDriver
