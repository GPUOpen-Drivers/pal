/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

#pragma once

#include <ddIoCtlDevice.h>
#include <winKernel/ddIoCtlDefines.h>
#include <ddPlatform.h>

namespace DevDriver
{

class WinKmIoCtlDevice final : public IIoCtlDevice
{
public:
    WinKmIoCtlDevice() {}
    ~WinKmIoCtlDevice()
    {
        Destroy();
    }

    DD_NODISCARD
    Result Initialize() override;

    void Destroy() override;

    // ioctl interface to devdriver (via amdlog)
    // ddCommandCode: DevDriver command number (see DevModeCmd type)
    // bufferSize   : Size of command-specific buffer to provide and receive command data
    // pBuffer      : Pointer to command-specific buffer to provide and receive command data
    DD_NODISCARD
    virtual Result IoCtl(
        uint32 ddCommandCode,
        size_t bufferSize,
        void*  pBuffer
    ) override;

    DD_NODISCARD
    virtual Result InDirectIoCtl(
        size_t inSize,
        void*  pInBuffer,
        size_t outSize,
        void*  pOutBuffer
    ) override;

    DD_NODISCARD
    Result IoCtl(
        IoCtlType type,
        size_t    inSize,
        void*     pInBuffer,
        size_t    outSize,
        void*     pOutBuffer
    ) override;

private:
    HANDLE m_hDevice = INVALID_HANDLE_VALUE;
};
} // DevDriver
