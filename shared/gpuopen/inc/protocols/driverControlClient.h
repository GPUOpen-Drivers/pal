/* Copyright (c) 2021 Advanced Micro Devices, Inc. All rights reserved. */

#pragma once

#include "legacyProtocolClient.h"
#include "protocols/driverControlProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace DriverControlProtocol
    {
        class DriverControlClient : public LegacyProtocolClient
        {
        public:
            explicit DriverControlClient(IMsgChannel* pMsgChannel);
            ~DriverControlClient();

            // Pauses execution inside the driver
            Result PauseDriver();

            // Resumes execution inside the driver
            Result ResumeDriver();

            // Steps the driver the requested number of steps
            Result StepDriver(uint32 numSteps);

            // Advances the current driver state to the next state in the initialization sequence.
            Result AdvanceDriverState(DriverStatus* pNewState);

            // Returns the number of gpus the driver is managing.
            Result QueryNumGpus(uint32* pNumGpus);

            // Returns the current device clock mode
            Result QueryDeviceClockMode(uint32 gpuIndex, DeviceClockMode* pClockMode);

            // Sets the current device clock mode
            Result SetDeviceClockMode(uint32 gpuIndex, DeviceClockMode clockMode);

#if DD_VERSION_SUPPORTS(GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION)
            // Returns the device clock values in MHz for the given device clock mode.
            Result QueryDeviceClock(uint32 gpuIndex, DeviceClockMode clockMode, float* pGpuClock, float* pMemClock);
#else
            // Returns the current device clock values in MHz
            Result QueryDeviceClock(uint32 gpuIndex, float* pGpuClock, float* pMemClock);

            // Returns the max device clock values in MHz
            Result QueryMaxDeviceClock(uint32 gpuIndex, float* pMaxGpuClock, float* pMaxMemClock);
#endif

            // Returns the current status of the driver.
            Result QueryDriverStatus(DriverStatus* pDriverStatus);

            // Waits until the driver finishes initialization.
            Result WaitForDriverInitialization(uint32 timeoutInMs);

            // Returns a ClientInfo struct populated for the connected client.
            Result QueryClientInfo(ClientInfoStruct* pClientInfo);

            // Notifies the driver that it will be ignored by tools
            //
            // This allows server-side implementations to reverse their initialization logic during the platform init
            // phase.
            Result IgnoreDriver();

        private:
            Result SendDriverControlPayload(const SizedPayloadContainer& container,
                                            uint32                       timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                            uint32                       retryInMs   = kDefaultRetryTimeoutInMs);
            Result ReceiveDriverControlPayload(SizedPayloadContainer* pContainer,
                                               uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                               uint32                 retryInMs   = kDefaultRetryTimeoutInMs);
            Result TransactDriverControlPayload(SizedPayloadContainer* pContainer,
                                                uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                                uint32                 retryInMs   = kDefaultRetryTimeoutInMs);

#if DD_VERSION_SUPPORTS(GPUOPEN_DRIVER_CONTROL_QUERY_CLOCKS_BY_MODE_VERSION)
            // Returns the current device clock values in MHz
            Result QueryDeviceClock(uint32 gpuIndex, float* pGpuClock, float* pMemClock);
#endif
        };
    }
} // DevDriver
