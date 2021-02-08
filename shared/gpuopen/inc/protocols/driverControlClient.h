/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2019-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
/**
***********************************************************************************************************************
* @file  driverControlClient.h
* @brief Developer Driver Control Client Interface
***********************************************************************************************************************
*/

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
