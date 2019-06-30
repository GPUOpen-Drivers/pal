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

namespace DevDriver
{

Result DevModeControlDevice::Initialize(DevModeBusType busType)
{
    Result result = Result::Error;

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
            case DevModeBusType::Auto:
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
            m_pIoCtlDevice = pIoCtlDevice;
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

Result DevModeControlDevice::MakeDevModeRequestRaw(
    DevModeCmd cmd,
    size_t     bufferSize,
    void*      pBuffer)
{
    // This function should never be called when we have a null IoCtl device pointer.
    // This could only happen if someone called this function before this object has been initialized.
    DD_ASSERT(m_pIoCtlDevice != nullptr);

    return m_pIoCtlDevice->IoCtl(static_cast<uint32>(cmd), bufferSize, pBuffer);
}

}
