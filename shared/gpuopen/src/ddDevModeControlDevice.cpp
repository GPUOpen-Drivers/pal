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
/**
***********************************************************************************************************************
* @file ddDevModeControlDevice.cpp
* @brief Implementation file for message channel code.
***********************************************************************************************************************
*/

#include <ddDevModeControlDevice.h>
#include <ddDevModeControlCmds.h>

#include <ddPlatform.h>

namespace DevDriver
{

Result DevModeControlDevice::Initialize(DevModeBusType busType)
{
    Result result = Result::Error;

    // If the user asked for an "auto" bus type, each platform picks its own
    // default and then follows the standard logic below (in the switch).
    if (busType == DevModeBusType::Auto)
    {
        // Posix platforms default to a user mode bus
        busType = DevModeBusType::UserMode;
    }

    // Prevent double initialization
    if (m_pIoCtlDevice == nullptr)
    {
        IIoCtlDevice* pIoCtlDevice = nullptr;

        switch (busType)
        {
            case DevModeBusType::UserMode:
            {
                result = Result::Unavailable;
                break;
            }

            case DevModeBusType::KernelMode:
            {
                result = Result::Unavailable;
                break;
            }

            default:
            {
                DD_ASSERT_ALWAYS();
                break;
            }
        }

        if (result == Result::Success)
        {
            result = pIoCtlDevice->Initialize();
            if (result != Result::Success)
            {
                DD_DELETE(pIoCtlDevice, m_allocCb);
            }
        }

        if (result == Result::Success)
        {
            m_pIoCtlDevice    = pIoCtlDevice;
            m_ioCtlDeviceType = busType;
        }
    }

    return result;
}

void DevModeControlDevice::Destroy()
{
    if (m_pIoCtlDevice != nullptr)
    {
        m_pIoCtlDevice->Destroy();
        DD_DELETE(m_pIoCtlDevice, m_allocCb);
        m_pIoCtlDevice = nullptr;
    }
}

// On Um bus types, we may need to do additional work to map the shared buffers for certain DevMode commands.
// This helper method maps a single buffer between UM clients.
// On failure, pQueue is not modified.
static Result MapSharedBufferUmToUm(QueueInfo* pQueue)
{
    Result result = Result::InvalidParameter;

    if ((pQueue != nullptr) && (pQueue->sharedBuffer.hSharedBufferView != NULL))
    {
        result = Result::Success;
    }

    if (result == Result::Success)
    {
        // @TODO: For some reason, the existing kernel implementation returns the process local shared buffer
        //        handle in the hSharedBufferView field instead of the hSharedBufferObject field. This should be
        //        cleaned up in the future but it's being left as-is for now to avoid regressions.
        const Handle hSharedQueueBuffer = pQueue->sharedBuffer.hSharedBufferView;
        const Handle hSharedQueueView   = Platform::Windows::MapSystemBufferView(hSharedQueueBuffer, pQueue->bufferSize);
        if (hSharedQueueView != NULL)
        {
            // Save the shared queue buffer handle in here so it can be closed after we unmap the buffer in the event
            // of a partial initialization failure.
            pQueue->sharedBuffer.hSharedBufferObject = hSharedQueueBuffer;
            pQueue->sharedBuffer.hSharedBufferView   = hSharedQueueView;
            result = Result::Success;
        }
        else
        {
            DD_PRINT(
                LogLevel::Error, "Failed to map queue for shared buffer communication. GLE = %d",
                GetLastError());
            result = Result::Error;
        }
    }

    return result;
}

// On Um bus types, we may need to do additional work to map the shared buffers for certain DevModeCmds.
// This helper method tears down that work in the event of partial failure.
static Result UnmapSharedBufferUmToUm(QueueInfo* pQueue)
{
    Result result = Result::InvalidParameter;

    if (pQueue != nullptr)
    {
        // We pass an invalid handle as the buffer object here since that parameter isn't relevant for usermode.
        Platform::Windows::UnmapBufferView(kInvalidHandle,
                                           pQueue->sharedBuffer.hSharedBufferView);

        // Close the shared buffer object
        Platform::Windows::CloseSharedBuffer(pQueue->sharedBuffer.hSharedBufferObject);

        pQueue->sharedBuffer.hSharedBufferObject = kInvalidHandle;
        pQueue->sharedBuffer.hSharedBufferView   = kInvalidHandle;

        result = Result::Success;
    }

    return result;
}

Result DevModeControlDevice::HandlePostIoCtlWork(DevModeCmd cmd, void* pBuffer)
{
    Result result = Result::Success;

    if (m_ioCtlDeviceType == DevModeBusType::UserMode)
    {
        switch (cmd)
        {
            // Since this is Um to Um communication, we need to map both the send and receive queue into our address space.
        case DevModeCmd::RegisterClient:
        {
            auto* pRequest = reinterpret_cast<RegisterClientRequest*>(pBuffer);
            result = MapSharedBufferUmToUm(&pRequest->output.sendQueue);
            if (result == Result::Success)
            {
                MapSharedBufferUmToUm(&pRequest->output.receiveQueue);
            }
            else
            {
                // We were only able to initialize one of the two queues - unmap the old one and reset it.
                DD_UNHANDLED_RESULT(UnmapSharedBufferUmToUm(&pRequest->output.receiveQueue));
            }
            break;
        }

        // Similar to above
        case DevModeCmd::RegisterRouter:
        {
            auto* pRequest = reinterpret_cast<RegisterRouterRequest*>(pBuffer);
            result = MapSharedBufferUmToUm(&pRequest->output.sendQueue);
            if (result == Result::Success)
            {
                MapSharedBufferUmToUm(&pRequest->output.receiveQueue);
            }
            else
            {
                DD_UNHANDLED_RESULT(UnmapSharedBufferUmToUm(&pRequest->output.receiveQueue));
            }
            break;
        }

        default:
            // Other commands have no post-work to do.
            break;
        }
    }
    else
    {
        // There's nothing to do, so nothing can fail.
    }

    return result;
}

Result DevModeControlDevice::MakeDevModeRequestRaw(
    DevModeCmd cmd,
    size_t     bufferSize,
    void*      pBuffer)
{
    // This function should never be called when we have a null IoCtl device pointer.
    // This method is private, so this is a programmer error that can fail hard.
    DD_ASSERT(m_pIoCtlDevice != nullptr);

    Result result = m_pIoCtlDevice->IoCtl(static_cast<uint32>(cmd), bufferSize, pBuffer);
    if (result == Result::Success)
    {
        result = HandlePostIoCtlWork(cmd, pBuffer);
    }
    return result;
}

}
