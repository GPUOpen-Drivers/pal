/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "ddPlatform.h"
#include "ddAmdLogInterface.h"

namespace DevDriver
{
class IIoCtlDevice;

// This callback allows the Logger to use the UMDs escape code paths rather than implement it
// directly in DevDriver.
typedef Result (*pfnAmdlogEscapeCb)(uint32_t gpuIdx,    // [in] GPU Index
                                    void*    pUserdata, // [in] Userdata pointer
                                    void*    pData,     // [in] Pointer to the log info
                                    size_t   dataSize); // [in] Size of the data

// Helper structure for pfnAmdlogEscapeCb
struct AmdLogEscapeCb
{
    void*             pUserData;   // [in] Userdata pointer
    pfnAmdlogEscapeCb pfnCallback; // [in] Pointer to a data callback function
};

class AmdLogLogger
{
public:
    AmdLogLogger(const AllocCb& allocCb, const AmdLogEscapeCb& escapeCb);
    ~AmdLogLogger();

    Result WriteAmdlogData(uint32_t logFlags, AmdlogEventId eventId, void* pData, size_t dataSize);
    Result WriteAmdlogString(uint32_t logFlags, const char* pFormat, ...);

protected:

    Result Init();
    Result WriteDataInternal(AmdLogEventInfo* pEventInfo);

    IIoCtlDevice*          m_pIoCtlDevice;
    AllocCb                m_allocCb;
    AmdLogEscapeCb         m_escapeCb;
    bool                   m_isUWPApp;
    bool                   m_isInit;
};

} // DevDriver
