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
/**
***********************************************************************************************************************
* @file  ddDevModeControlDevice.h
* @brief Cross platform interface to utility driver
***********************************************************************************************************************
*/
#pragma once

#include <gpuopen.h>
#include <ddPlatform.h>
#include <ddDevModeControl.h>

namespace DevDriver
{

class IIoCtlDevice;

/// Provides a control interface for configuring the developer mode bus.
class DevModeControlDevice
{
public:
    DevModeControlDevice(const AllocCb& allocCb)
        : m_allocCb(allocCb)
    {
    }

    ~DevModeControlDevice()
    {
    }

    // Lifetime management functions
    DD_NODISCARD
    Result Initialize(DevModeBusType busType);

    void Destroy();

    /// Platform-agnostic call into the devmode device.
    ///
    /// This is a convience overload to prevent errors with DevModeCmds.
    /// Prefer this to the non-templated overload.
    template <typename Request>
    DD_NODISCARD
    Result MakeDevModeRequest(
        Request* pInOutBuffer
    )
    {
        Result result = Result::InvalidParameter;

        // Buffer must be valid and contain a DevModeResponseHeader.
        if (IsInitialized() && (pInOutBuffer != nullptr))
        {
            // Make sure the header is the right offset. This is a compile-time check.
            static_assert(offsetof(Request, header) == 0, "Expected header field at offset 0");

            // Make sure the header is the right type. This is a compile-time check.
            const DevModeResponseHeader& header = pInOutBuffer->header;
            DD_UNUSED(header);

            result = MakeDevModeRequestRaw(
                Request::kCmd,
                sizeof(*pInOutBuffer),
                pInOutBuffer
            );
        }

        return result;
    }

private:
    /// Platform-agnostic call into the devmode device.
    ///
    /// Prefer calling the typed-wrapper instead of this whenever possible.
    DD_NODISCARD
    Result MakeDevModeRequestRaw(
        DevModeCmd cmd,
        size_t     bufferSize,
        void*      pBuffer
    );

    DD_NODISCARD
    Result HandlePostIoCtlWork(DevModeCmd cmd, void* pBuffer);

    bool IsInitialized() const
    {
        // This should not happen. Auto is resolved to UserMode or KernelMode at init time.
        DD_ASSERT(m_ioCtlDeviceType != DevModeBusType::Auto);
        return ((m_pIoCtlDevice != nullptr) && (m_ioCtlDeviceType != DevModeBusType::Unknown));
    }

    // Allocation callbacks
    AllocCb        m_allocCb;

    // Device to issue ioctl commands through. May be user-mode or kernel mode depending on the platform.
    IIoCtlDevice*  m_pIoCtlDevice = nullptr;

    // Type of IoCtlDevice stored in m_pIoCtlDevice
    // This may be Unknown, UserMode, or KernelMode, but will never be Auto. Unknown represents and uninitialized object.
    DevModeBusType m_ioCtlDeviceType = DevModeBusType::Unknown;
};

}
