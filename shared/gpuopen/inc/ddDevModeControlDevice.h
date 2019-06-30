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
        , m_pIoCtlDevice(nullptr)
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
    template <typename Data>
    DD_NODISCARD
        Result MakeDevModeRequest(
            Data* pInOutBuffer
        )
    {
        DD_ASSERT(pInOutBuffer != nullptr);
        // Make sure the header is the right offset.
        static_assert(offsetof(Data, header) == 0, "Expected header field at offset 0");
        // Make sure the header is the right type.
        const DevModeResponseHeader& header = pInOutBuffer->header;
        DD_UNUSED(header);

        return MakeDevModeRequestRaw(
            Data::kCmd,
            sizeof(*pInOutBuffer),
            pInOutBuffer
        );
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

    AllocCb       m_allocCb;
    IIoCtlDevice* m_pIoCtlDevice;
};

}
